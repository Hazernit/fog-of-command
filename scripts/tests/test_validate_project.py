"""Negative static-contract tests. No UE, network, Git writes or root edits."""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("validate_project", ROOT / "scripts/validate_project.py")
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class StaticContractTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="foc-static-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for relative in validator.REQUIRED_FILES:
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if relative != validator.MAP:
                shutil.copyfile(ROOT / relative, destination)
        self.asset = b"static contract fixture, not a valid UE map\x00"
        (self.root / validator.MAP).write_bytes(self.asset)
        self.tracked = set(validator.REQUIRED_FILES)
        digest = hashlib.sha256(self.asset).hexdigest()
        self.pointer = ("version https://git-lfs.github.com/spec/v1\n"
                        "oid sha256:" + digest + "\nsize " + str(len(self.asset)) + "\n").encode("ascii")
        self.attributes = {"filter": "lfs", "diff": "lfs", "merge": "lfs", "text": "unset"}
        self.patch_git = patch.object(validator, "git_bytes", side_effect=self.git_bytes)
        self.patch_git.start()
        self.addCleanup(self.patch_git.stop)

    def git_bytes(self, root, *args):
        if args == ("ls-files", "-z"):
            return ("\0".join(sorted(self.tracked)) + "\0").encode("utf-8")
        if args[0] == "check-attr":
            return "".join(args[-1] + "\0" + key + "\0" + value + "\0" for key, value in self.attributes.items()).encode("utf-8")
        if args[:2] == ("cat-file", "-s"):
            return str(len(self.pointer)).encode("ascii")
        if args[:2] == ("cat-file", "blob"):
            return self.pointer
        raise AssertionError("unexpected read-only Git request")

    def replace(self, relative, old, new):
        path = self.root / relative
        source = path.read_text(encoding="utf-8-sig")
        self.assertIn(old, source)
        path.write_text(source.replace(old, new), encoding="utf-8")

    def assert_error(self, fragment):
        errors = validator.validate(self.root)
        self.assertTrue(any(fragment in error for error in errors), errors)

    def test_valid_contract(self):
        self.assertEqual([], validator.validate(self.root))

    def test_wrong_sdk_pin_is_rejected(self):
        self.replace("Source/FogOfCommandEditor.Target.cs", '"10.0.22621.0"', '"10.0.26100.0"')
        self.assert_error("pin mismatch for WindowsPlatform.WindowsSdkVersion")

    def test_commenting_out_pin_is_not_accepted(self):
        self.replace("Source/FogOfCommand.Target.cs", "WindowsPlatform.CompilerVersion =", "// WindowsPlatform.CompilerVersion =")
        self.assert_error("pin mismatch for WindowsPlatform.CompilerVersion")

    def test_later_override_is_not_accepted(self):
        path = self.root / "Source/FogOfCommand.Target.cs"
        with path.open("a", encoding="utf-8") as stream:
            stream.write('\nWindowsPlatform.CompilerVersion = "14.51.36260";\n')
        self.assert_error("pin mismatch for WindowsPlatform.CompilerVersion")

    def test_invalid_descriptor_and_duplicate_keys_are_rejected(self):
        (self.root / "FogOfCommand.uproject").write_text('{"EngineAssociation":"5.8","EngineAssociation":"5.3"}', encoding="utf-8")
        self.assert_error("invalid project descriptor")

    def test_changed_module_is_rejected(self):
        path = self.root / "FogOfCommand.uproject"
        descriptor = json.loads(path.read_text(encoding="utf-8-sig"))
        descriptor["Modules"][0]["Type"] = "Editor"
        path.write_text(json.dumps(descriptor), encoding="utf-8")
        self.assert_error("Runtime/Default module")

    def test_duplicate_module_is_rejected(self):
        path = self.root / "FogOfCommand.uproject"
        descriptor = json.loads(path.read_text(encoding="utf-8-sig"))
        descriptor["Modules"].append({"Name": "FogOfCommand", "Type": "Editor", "LoadingPhase": "Default"})
        path.write_text(json.dumps(descriptor), encoding="utf-8")
        self.assert_error("Runtime/Default module")

    def test_reenabled_file_server_is_rejected(self):
        path = self.root / "FogOfCommand.uproject"
        descriptor = json.loads(path.read_text(encoding="utf-8-sig"))
        descriptor["Plugins"][0]["Enabled"] = True
        path.write_text(json.dumps(descriptor), encoding="utf-8")
        self.assert_error("AndroidFileServer")

    def test_wrong_default_map_is_rejected(self):
        self.replace("Config/DefaultEngine.ini", "GameDefaultMap=/Game/Maps/Startup.Startup", "GameDefaultMap=/Engine/Maps/Entry.Entry")
        self.assert_error("default map/GameMode mismatch: GameDefaultMap")

    def test_missing_map_is_rejected(self):
        (self.root / validator.MAP).unlink()
        self.assert_error("required file is missing: " + validator.MAP)

    def test_untracked_required_file_is_rejected(self):
        self.tracked.remove("Source/FogOfCommandEditor.Target.cs")
        self.assert_error("required file is not tracked")

    def test_raw_committed_asset_is_rejected(self):
        self.pointer = b"raw binary map instead of an LFS pointer"
        self.assert_error("committed UE/asset blob is not an LFS pointer")

    def test_unhydrated_lfs_pointer_is_rejected(self):
        (self.root / validator.MAP).write_bytes(self.pointer)
        self.assert_error("missing hydration or size mismatch")

    def test_same_size_asset_corruption_is_rejected(self):
        (self.root / validator.MAP).write_bytes(b"X" * len(self.asset))
        self.assert_error("LFS asset hash mismatch")

    def test_removed_lfs_attribute_is_rejected(self):
        self.attributes["filter"] = "unspecified"
        self.assert_error("LFS attributes mismatch")

    def test_tracked_generated_output_is_rejected(self):
        self.tracked.add("Saved/Config/WindowsEditor/DefaultInput.ini")
        self.assert_error("forbidden tracked output/private file")

    def test_domain_actor_dependency_is_rejected(self):
        path = self.root / validator.DOMAIN_FILES[1]
        with path.open("a", encoding="utf-8") as stream:
            stream.write('\n#include "GameFramework/Actor.h"\n')
        self.assert_error("domain Actor/world/UI dependency")

    def test_domain_reflection_dependency_is_rejected(self):
        path = self.root / validator.DOMAIN_FILES[1]
        with path.open("a", encoding="utf-8") as stream:
            stream.write('\nUCLASS() class UUnitState : public UObject {};\n')
        self.assert_error("domain Actor/reflection/Tick boundary")

    def test_domain_tick_is_rejected(self):
        path = self.root / validator.DOMAIN_FILES[2]
        with path.open("a", encoding="utf-8") as stream:
            stream.write('\nvoid Tick(float DeltaSeconds) {}\n')
        self.assert_error("domain Actor/reflection/Tick boundary")

    def test_orders_actor_dependency_is_rejected(self):
        path = self.root / "Source/FogOfCommand/Public/Domain/Orders.h"
        with path.open("a", encoding="utf-8") as stream:
            stream.write('\n#include "Components/ActorComponent.h"\n')
        self.assert_error("domain Actor/world/UI dependency")

    def test_missing_order_implementation_is_rejected(self):
        (self.root / "Source/FogOfCommand/Private/Domain/Orders.cpp").unlink()
        self.assert_error("required file is missing: Source/FogOfCommand/Private/Domain/Orders.cpp")

    def test_events_actor_dependency_is_rejected(self):
        path = self.root / "Source/FogOfCommand/Public/Domain/SimulationEvents.h"
        with path.open("a", encoding="utf-8") as stream:
            stream.write('\n#include "Engine/World.h"\n')
        self.assert_error("domain Actor/world/UI dependency")

    def test_missing_event_implementation_is_rejected(self):
        relative = "Source/FogOfCommand/Private/Domain/SimulationEvents.cpp"
        (self.root / relative).unlink()
        self.assert_error("required file is missing: " + relative)

    def test_missing_debug_build_guard_is_rejected(self):
        relative = validator.DEBUG_FILES[1]
        self.replace(relative, "#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST", "#if 1")
        self.assert_error("developer debug build guard missing")

    def test_missing_debug_opt_in_is_rejected(self):
        relative = validator.DEBUG_FILES[1]
        self.replace(relative, 'TEXT("FocEventDebug")', 'TEXT("UnrelatedFlag")')
        self.assert_error("developer debug explicit opt-in flag missing")


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Static project/Git contract checks; does not compile C++ or launch Unreal.

Requires Python's standard library and Git. LFS assets must be hydrated and their
HEAD blobs must be LFS pointers. No files, Git configuration or assets are edited.
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import re
import subprocess
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]
MAP = "Content/Maps/Startup.umap"
DOMAIN_FILES = (
    "Source/FogOfCommand/Public/Domain/UnitIdentifiers.h",
    "Source/FogOfCommand/Public/Domain/UnitState.h",
    "Source/FogOfCommand/Private/Domain/UnitIdentifiers.cpp",
    "Source/FogOfCommand/Private/Domain/UnitState.cpp",
)
REQUIRED_FILES = (
    "FogOfCommand.uproject", "Config/DefaultEngine.ini", "Config/DefaultGame.ini",
    "Source/FogOfCommand.Target.cs", "Source/FogOfCommandEditor.Target.cs",
    "Source/FogOfCommand/FogOfCommand.Build.cs",
    "Source/FogOfCommand/Private/FogOfCommand.cpp",
    "Source/FogOfCommand/Private/FogOfCommandGameMode.cpp",
    "Source/FogOfCommand/Public/FogOfCommandGameMode.h",
    "docs/ENGINE_VERSION.md", MAP,
) + DOMAIN_FILES
LFS_SUFFIXES = {".umap", ".uasset", ".ubulk", ".uexp", ".fbx", ".blend", ".wav"}
BANNED_PARTS = {"binaries", "intermediate", "saved", "deriveddatacache", ".vs", ".idea", ".venv", ".tmp", "__pycache__"}
BANNED_SUFFIXES = {".log", ".sln", ".pyc", ".tmp", ".bak", ".dmp", ".mdmp", ".etl", ".har", ".pem", ".key", ".pfx", ".p12"}
POINTER = re.compile(rb"version https://git-lfs.github.com/spec/v1\noid sha256:([0-9a-f]{64})\nsize ([0-9]+)\n?\Z")


def git_bytes(root: Path, *args: str) -> bytes:
    """Read Git metadata without echoing potentially private Git stderr."""
    try:
        result = subprocess.run(["git", "-C", str(root), *args], capture_output=True, check=False)
    except OSError as exc:
        raise ValueError("Git executable is unavailable") from exc
    if result.returncode:
        raise ValueError("Git metadata query failed: " + args[0])
    return result.stdout


def uncomment(source: str) -> str:
    # A narrow contract check, not a C++/C# parser. Ignore commented-out pins.
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.DOTALL)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key: " + key)
        result[key] = value
    return result


def validate(root: Path) -> list[str]:
    errors = []

    def require(condition, message):
        if not condition:
            errors.append(message)

    def read(relative):
        try:
            return (root / relative).read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError):
            errors.append("missing/unreadable text file: " + relative)
            return ""

    try:
        tracked = set(git_bytes(root, "ls-files", "-z").decode("utf-8").strip("\0").split("\0")) - {""}
    except (ValueError, UnicodeError) as exc:
        return [str(exc)]
    for relative in REQUIRED_FILES:
        require(relative in tracked, "required file is not tracked: " + relative)
        require((root / relative).is_file(), "required file is missing: " + relative)
    for relative in sorted(tracked):
        path = PurePosixPath(relative)
        parts = {part.casefold() for part in path.parts}
        name = path.name.casefold()
        banned = bool(parts & BANNED_PARTS) or path.suffix.casefold() in BANNED_SUFFIXES
        banned |= (name == ".env" or (name.startswith(".env.") and name != ".env.example"))
        banned |= name.startswith(("id_rsa", "id_ed25519"))
        banned |= "diagnostics/raw" in relative.casefold() or "diagnostics/private" in relative.casefold()
        require(not banned, "forbidden tracked output/private file: " + relative)

    try:
        descriptor = json.loads(read("FogOfCommand.uproject"), object_pairs_hook=unique_object)
        require(descriptor.get("FileVersion") == 3, "descriptor FileVersion must be 3")
        require(descriptor.get("EngineAssociation") == "5.8", "descriptor EngineAssociation must be 5.8")
        modules = descriptor.get("Modules", [])
        named_modules = [m for m in modules if isinstance(m, dict) and m.get("Name") == "FogOfCommand"] if isinstance(modules, list) else []
        require(len(named_modules) == 1 and named_modules[0].get("Type") == "Runtime" and named_modules[0].get("LoadingPhase") == "Default",
                "descriptor must declare one FogOfCommand Runtime/Default module")
        plugins = descriptor.get("Plugins", [])
        file_servers = [p for p in plugins if isinstance(p, dict) and p.get("Name") == "AndroidFileServer"] if isinstance(plugins, list) else []
        require(len(file_servers) == 1 and file_servers[0].get("Enabled") is False,
                "AndroidFileServer must remain explicitly disabled")
    except (ValueError, AttributeError, TypeError):
        errors.append("invalid project descriptor JSON/structure")

    targets = (("Source/FogOfCommand.Target.cs", "FogOfCommandTarget", "Game"),
               ("Source/FogOfCommandEditor.Target.cs", "FogOfCommandEditorTarget", "Editor"))
    for relative, class_name, target_type in targets:
        source = uncomment(read(relative))
        require(bool(re.search(r"class\s+" + class_name + r"\s*:\s*TargetRules\b", source)), relative + ": target class mismatch")
        require(bool(re.search(r'SupportedPlatforms\(\s*"Win64"\s*\)', source)), relative + ": expected Win64 platform")
        pins = {
            "Type": "TargetType." + target_type,
            "DefaultBuildSettings": "BuildSettingsVersion.V7",
            "IncludeOrderVersion": "EngineIncludeOrderVersion.Unreal5_8",
            "WindowsPlatform.Compiler": "WindowsCompiler.VisualStudio2022",
            "WindowsPlatform.CompilerVersion": '"14.44.35229"',
            "WindowsPlatform.ToolchainVersion": '"14.44.35229"',
            "WindowsPlatform.WindowsSdkVersion": '"10.0.22621.0"',
        }
        for property_name, expected in pins.items():
            values = re.findall(r"\b" + re.escape(property_name) + r"\s*=\s*([^;]+);", source)
            require([v.strip() for v in values] == [expected], relative + ": pin mismatch for " + property_name)
        require(bool(re.search(r'ExtraModuleNames\.Add\(\s*"FogOfCommand"\s*\)', source)), relative + ": module registration missing")
    module = uncomment(read("Source/FogOfCommand/FogOfCommand.Build.cs"))
    require(bool(re.search(r"class\s+FogOfCommand\s*:\s*ModuleRules\b", module)), "FogOfCommand module rules class mismatch")
    require(all('"' + name + '"' in module for name in ("Core", "CoreUObject", "Engine")), "core module dependencies are missing")
    module_cpp = uncomment(read("Source/FogOfCommand/Private/FogOfCommand.cpp"))
    require(bool(re.search(r'IMPLEMENT_PRIMARY_GAME_MODULE\(\s*FDefaultGameModuleImpl\s*,\s*FogOfCommand\s*,\s*"FogOfCommand"\s*\)', module_cpp)), "primary game module registration mismatch")

    # A narrow dependency boundary, not a C++ compiler or semantic proof. Keep
    # FOC-004 values independent of Actor/world/UI/reflection and per-frame Tick.
    for relative in DOMAIN_FILES:
        source = uncomment(read(relative))
        includes = re.findall(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', source, re.MULTILINE)
        forbidden_include = any(
            name.startswith(("Engine/", "GameFramework/", "Components/", "UObject/", "Blueprint/", "Widgets/"))
            or name in {"Editor.h", "Engine.h"} for name in includes
        )
        require(not forbidden_include, "domain Actor/world/UI dependency: " + relative)
        forbidden_symbol = re.search(r'\b(?:AActor|UWorld|UObject)\b|\bU(?:CLASS|STRUCT|PROPERTY|FUNCTION)\s*\(|\bTick\s*\(', source)
        require(not forbidden_symbol, "domain Actor/reflection/Tick boundary: " + relative)

    try:
        ini = configparser.ConfigParser(interpolation=None, strict=True)
        ini.read_string(read("Config/DefaultEngine.ini"))
        section = "/Script/EngineSettings.GameMapsSettings"
        for key, expected in {
            "EditorStartupMap": "/Game/Maps/Startup.Startup",
            "GameDefaultMap": "/Game/Maps/Startup.Startup",
            "GlobalDefaultGameMode": "/Script/FogOfCommand.FogOfCommandGameMode",
        }.items():
            require(ini.get(section, key, fallback="") == expected, "default map/GameMode mismatch: " + key)
        section = "/Script/WindowsTargetPlatform.WindowsTargetSettings"
        require(ini.get(section, "DefaultGraphicsRHI", fallback="") == "DefaultGraphicsRHI_DX12", "Windows RHI must be DX12")
        require(ini.get(section, "+D3D12TargetedShaderFormats", fallback="") == "PCD3D_SM5", "baseline shader format must be SM5")
    except configparser.Error:
        errors.append("invalid/duplicate DefaultEngine.ini configuration")
    baseline = read("docs/ENGINE_VERSION.md")
    for pin in ("PIN: 5.8.4 CL58902052", "VS2022 17.14.42", "MSVC 14.44.35229", "10.0.22621.0"):
        require(pin in baseline, "accepted baseline documentation mismatch: " + pin)

    for relative in sorted(tracked):
        if PurePosixPath(relative).suffix.casefold() not in LFS_SUFFIXES:
            continue
        try:
            attrs = git_bytes(root, "check-attr", "-z", "filter", "diff", "merge", "text", "--", relative).decode("utf-8").strip("\0").split("\0")
            values = {attrs[i + 1]: attrs[i + 2] for i in range(0, len(attrs), 3)}
            require(values == {"filter": "lfs", "diff": "lfs", "merge": "lfs", "text": "unset"}, "LFS attributes mismatch: " + relative)
            size = int(git_bytes(root, "cat-file", "-s", "HEAD:" + relative))
            if size > 1024:
                errors.append("committed UE/asset blob is not an LFS pointer: " + relative)
                continue
            pointer = POINTER.fullmatch(git_bytes(root, "cat-file", "blob", "HEAD:" + relative))
            if not pointer:
                errors.append("committed UE/asset blob is not an LFS pointer: " + relative)
                continue
            asset = root / relative
            if not asset.is_file():
                errors.append("LFS working asset is missing: " + relative)
                continue
            if asset.stat().st_size != int(pointer.group(2)):
                errors.append("LFS asset missing hydration or size mismatch: " + relative)
                continue
            digest = hashlib.sha256()
            with asset.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            require(digest.hexdigest() == pointer.group(1).decode("ascii"), "LFS asset hash mismatch: " + relative)
        except (OSError, ValueError, UnicodeError, IndexError):
            errors.append("cannot verify committed LFS pointer/asset: " + relative)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT, help="repository root (default: this script's repository)")
    args = parser.parse_args()
    errors = validate(args.root.resolve())
    if errors:
        print("STATIC FAIL:")
        for error in errors:
            print(" - " + error)
    else:
        print("STATIC PASS: descriptor/module/target pins, map config, domain boundary, tracked files and committed LFS assets")
    print("Unreal C++ build / project Automation / Editor / PIE: NOT RUN by this static validator")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())

# FOC-002 Startup map

Use the pinned UE 5.8.4 **after building FogOfCommandEditor**. The scripts use the
built-in Python Editor Script Plugin for this process only. Do not enable Python
permanently in the `.uproject`, install Python packages, or run these commands in
an existing Editor with unsaved work.

`create_startup_map.py` creates only `Content/Maps/Startup.umap`. It uses Engine
Cube/Cylinder/Sphere meshes, a collidable 80-by-80-by-1 floor at Z=-50, dynamic
lighting/sky/fog and one PlayerStart. It creates no World Partition, external
actor packages, Starter Content, custom materials or gameplay assets. An existing
Startup map is refused by default; `--overwrite` explicitly rebuilds that map's
actors in place and still refuses partitioned/external-actor levels. Scene layout
and labels are reproducible; binary package bytes/GUIDs are not promised identical.

`validate_startup_map.py` loads the saved map without saving anything. It checks
startup/default-map settings, the native GameMode and DefaultPawn, map packaging,
all four meshes/transforms, floor bounds and Pawn/Visibility blocking, dynamic
lights/atmosphere/fog, one unobstructed PlayerStart over the floor and its facing.
This validates saved assets; successful Editor/PIE remains a separate runtime check.
It can also validate a map retrieved from Git LFS in a fresh checkout.

From a PowerShell in the repository, set `$UE` to the installed pinned engine root:

```powershell
$Project = (Resolve-Path '.\FogOfCommand.uproject').Path
$Editor = Join-Path $UE 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Script = (Resolve-Path '.\scripts\unreal\create_startup_map.py').Path.Replace('\', '/')
$ScriptArgument = '-script="' + $Script + '"'
& $Editor $Project -EnablePlugins=PythonScriptPlugin -run=pythonscript $ScriptArgument -unattended -nop4 -nosplash -NullRHI
if ($LASTEXITCODE -ne 0) { throw 'Startup map generation failed; inspect the commandlet log.' }

$Script = (Resolve-Path '.\scripts\unreal\validate_startup_map.py').Path.Replace('\', '/')
$ScriptArgument = '-script="' + $Script + '"'
& $Editor $Project -EnablePlugins=PythonScriptPlugin -run=pythonscript $ScriptArgument -unattended -nop4 -nosplash -NullRHI
if ($LASTEXITCODE -ne 0) { throw 'Saved Startup map validation failed.' }
```

For an intentional regeneration, the complete script value is
`'"' + $Script + '" --overwrite'`; put that value inside the commandlet's quoted
`-script=` argument. Do not use the flag to replace unrelated maps.

Require both a zero process exit code and the matching public marker:

- Creator: `FOC002_STARTUP_MAP_CREATED`.
- Saved-map validator: `FOC002_STARTUP_MAP_VALIDATION_PASS`.
- A validation exception starts with `FOC002_STARTUP_MAP_VALIDATION_FAIL`.

Use the usual OS language. Record the separate known ru-RU startup self-test
failures (#55) instead of changing global language settings to hide them.
Keep raw commandlet logs outside Git and review sanitized excerpts before publishing.
Git LFS must be applied before adding the generated `.umap` to a commit.

The candidate C++ signatures and CLI parser were inspected in the installed
UE 5.8.4 source. C++ reflection metadata alone does not prove a method is exposed
under the same Python name; confirm Python calls in the actual commandlet:

- `PythonScriptCommandlet.cpp`: parses `-Script=`, runs it unattended, returns
  nonzero when Python raises; `PythonScriptPlugin.cpp` supplies script `sys.argv`.
- `PluginManager.cpp`: supports process-only `EnablePlugins=`.
- `LevelEditorSubsystem.h/.cpp`: `NewLevel(AssetPath, false)`, `LoadLevel`,
  `SaveCurrentLevel`; `EditorActorSubsystem.h`: actor enumeration/spawn/destruction.
- `World.h`, `WorldSettings.h`, `Level.h`, `GameMapsSettings.h`: world settings,
  nonpartitioned/internal packaging and default map/GameMode properties.
- Component headers provide candidate mesh, collision, mobility and dynamic-light
  signatures. Actual Python names were then checked in a read-only commandlet.
- `PyGenUtil.cpp`: reflected method/property/enum Python names; `PyCore.cpp`
  provides `load_class`, `load_asset`, and `get_default_object`.

Read-only UE commandlet probes confirmed that `SoftObjectPath.export_text()` and
`SoftClassPath.export_text()` return the canonical map/class paths. They also
confirmed `CollisionChannel.ECC_PAWN` / `ECC_VISIBILITY` and the distinct enum
`CollisionResponseType.ECR_BLOCK`, including an actual component response.
`CollisionResponse` is a different Python type and must not be used for that enum.
The probes confirmed current-level/external-actor/world-settings access, the
DefaultPawn collision component and the dynamic-light methods used here.

Those probes and Python syntax checks do not constitute generated-map validation
or PIE acceptance. Record the actual creator/validator/PIE results in the FOC-002 PR.

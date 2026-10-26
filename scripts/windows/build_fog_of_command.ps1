<#
.SYNOPSIS
Builds the FogOfCommand Editor target with the accepted Windows baseline.
.DESCRIPTION
Requires installed UE 5.8.4 CL58902052 and an existing FogOfCommand project.
Does not clean/delete outputs, install software, edit Engine/global settings,
change language preferences, or launch Editor. A fresh checkout with no Binaries
or Intermediate is needed to claim a clean project build. Shared caches remain.
Raw logs and JSON contain private local paths: EvidenceRoot must be outside all
Git checkouts. Publish only a reviewed, sanitized summary of those local files.
.PARAMETER EngineRoot
Absolute local root of the installed UE 5.8.4 CL58902052 engine.
.PARAMETER ProjectRoot
Optional root containing FogOfCommand.uproject; defaults to this repository.
.PARAMETER EvidenceRoot
Absolute local evidence directory outside all Git checkouts and reparse points.
Each invocation creates a unique child directory and refuses to overwrite it.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EngineRoot,
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..\..'),
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EvidenceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-LocalFullPath([string]$Value) {
    # Batch-file arguments must not permit cmd.exe expansion or command injection.
    if ($Value -notmatch '^[A-Za-z]:[\\/]' -or $Value -match '["%\x21&|<>^\r\n]') {
        throw 'Use an absolute local drive path without cmd metacharacters.'
    }
    $fullPath = [IO.Path]::GetFullPath($Value)
    for ($current = $fullPath; $current; $current = [IO.Path]::GetDirectoryName($current)) {
        if (Test-Path -LiteralPath $current) {
            if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Paths through junctions/symlinks are not supported.'
            }
        }
    }
    if ($fullPath -eq [IO.Path]::GetPathRoot($fullPath)) { return $fullPath }
    return $fullPath.TrimEnd([char[]]'\/')
}

function Assert-OutsideGit([string]$Path) {
    for ($current = $Path; $current; $current = [IO.Path]::GetDirectoryName($current)) {
        if (Test-Path -LiteralPath (Join-Path $current '.git')) {
            throw 'Raw build evidence must be outside every Git checkout.'
        }
    }
}

function Test-IsWithin([string]$Path, [string]$Parent) {
    return $Path.Equals($Parent, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Parent.TrimEnd([char[]]'\/') + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Assert-EngineVersion($Version) {
    if ($Version.MajorVersion -ne 5 -or $Version.MinorVersion -ne 8 -or
        $Version.PatchVersion -ne 4 -or $Version.Changelist -ne 58902052) {
        throw 'Engine does not match the accepted UE 5.8.4 CL58902052 baseline; no build was launched.'
    }
}

function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}

$EngineRoot = Get-LocalFullPath $EngineRoot
$ProjectRoot = Get-LocalFullPath $ProjectRoot
$EvidenceRoot = Get-LocalFullPath $EvidenceRoot
Assert-OutsideGit $EvidenceRoot
if ((Test-IsWithin $EvidenceRoot $EngineRoot) -or (Test-IsWithin $EvidenceRoot $ProjectRoot)) {
    throw 'Evidence must be outside both the engine installation and project directory.'
}
if (Test-IsWithin $ProjectRoot $EngineRoot) {
    throw 'The project must not be inside the engine installation.'
}
$project = Join-Path $ProjectRoot 'FogOfCommand.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$versionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'
foreach ($required in @($build, $versionFile, $project, (Join-Path $ProjectRoot 'Source\FogOfCommandEditor.Target.cs'))) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw 'A required engine or FogOfCommand project file is missing.' }
}
$engineVersion = [IO.File]::ReadAllText($versionFile) | ConvertFrom-Json
Assert-EngineVersion $engineVersion
$descriptor = [IO.File]::ReadAllText($project) | ConvertFrom-Json
if ($descriptor.EngineAssociation -ne '5.8' -or 'FogOfCommand' -notin @($descriptor.Modules | ForEach-Object { $_.Name })) {
    throw 'FogOfCommand.uproject must associate with UE 5.8 and contain the FogOfCommand module.'
}
$runId = 'FOC002-build-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$out = Join-Path $EvidenceRoot $runId
if (Test-Path -LiteralPath $out) { throw 'Refusing to overwrite an existing build evidence run.' }
$arguments = @(
    'FogOfCommandEditor', 'Win64', 'Development', "-Project=$project",
    '-WaitMutex', '-NoHotReloadFromIDE', '-NoHotReload', '-MaxParallelActions=4',
    "-Log=$(Join-Path $out 'UnrealBuildTool.log')"
)
foreach ($value in @($build) + $arguments) {
    if ($value -match '["%\x21&|<>^\r\n]') { throw 'Unsafe character in build argument.' }
}
$binariesBefore = Test-Path -LiteralPath (Join-Path $ProjectRoot 'Binaries')
$intermediateBefore = Test-Path -LiteralPath (Join-Path $ProjectRoot 'Intermediate')
New-Item -ItemType Directory -Path $out | Out-Null
Write-Json (Join-Path $out 'pre-build.json') ([ordered]@{
    RunId = $runId; Project = $project; EngineRoot = $EngineRoot; EvidenceDirectory = $out
    EngineBuild = $engineVersion
    BinariesExistedBeforeBuild = $binariesBefore
    IntermediateExistedBeforeBuild = $intermediateBefore
    ProjectOutputsAbsentBeforeBuild = (!$binariesBefore -and !$intermediateBefore)
    OutputDeletion = $false; EngineRebuild = $false
    CacheState = 'Existing engine/shared/UBA caches are uncontrolled and are not cleared'
    ExpectedToolchain = 'VS2022 / MSVC 14.44.35229 / Windows SDK 10.0.22621.0; pinned in project target rules'
})
Write-Json (Join-Path $out 'command.json') @{ Executable = $build; Arguments = $arguments }
# Use the same quoted native batch-file invocation as the verified FOC-046 runner.
$commandLine = '""' + $build + '" ' + (($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ') + '"'
$started = [DateTimeOffset]::Now
$timer = [Diagnostics.Stopwatch]::StartNew()
$nativeExitCode = $null
$launchError = $null
try {
    $process = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d', '/s', '/c', $commandLine) `
        -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $out 'stdout.txt') `
        -RedirectStandardError (Join-Path $out 'stderr.txt')
    $nativeExitCode = $process.ExitCode
}
catch { $launchError = $_.Exception.Message }
finally { $timer.Stop() }

$selectedToolchain = $null
$toolchainStatus = 'NOT RUN - toolchain selection was not emitted in this build log'
$toolchainReadError = $null
$ubtLog = Join-Path $out 'UnrealBuildTool.log'
try {
    if (Test-Path -LiteralPath $ubtLog -PathType Leaf) {
        $logText = [IO.File]::ReadAllText($ubtLog)
        $selection = [regex]::Match($logText, '(?m)^Using Visual Studio (?:2022 )?(?<MSVC>\d+\.\d+\.\d+) toolchain \((?<ToolchainPath>[^\r\n]+?)\) and Windows (?<SDK>\d+\.\d+\.\d+\.\d+) SDK \((?<SdkPath>[^\r\n]+?)\)\.?\r?$')
        if ($selection.Success) {
            $selectedToolchain = [ordered]@{
                MSVC = $selection.Groups['MSVC'].Value
                WindowsSDK = $selection.Groups['SDK'].Value
                ToolchainPath = $selection.Groups['ToolchainPath'].Value
                SDKPath = $selection.Groups['SdkPath'].Value
            }
            $expected = $selectedToolchain.MSVC -eq '14.44.35229' -and
                $selectedToolchain.WindowsSDK -eq '10.0.22621.0' -and
                $selectedToolchain.ToolchainPath -match '[\\/]2022[\\/][^\\/]+[\\/]VC[\\/]Tools[\\/]MSVC[\\/]14\.44\.35207$'
            $toolchainStatus = if ($expected) { 'PASS' } else { 'FAIL - selected toolchain differs from the accepted baseline' }
        }
    }
}
catch { $toolchainStatus = 'NOT RUN - toolchain log could not be read'; $toolchainReadError = $_.Exception.Message }
$buildResult = if ($null -eq $nativeExitCode) { 'NOT RUN' } elseif ($nativeExitCode -eq 0) { 'PASS' } else { 'FAIL' }
# A native compile result and toolchain observation are separate facts. Return a
# wrapper failure on an observed pin mismatch while preserving the native code.
$wrapperExitCode = if ($null -eq $nativeExitCode) { 1 } elseif ($nativeExitCode -ne 0) { $nativeExitCode } elseif ($toolchainStatus -like 'FAIL*') { 1 } else { 0 }
$result = [ordered]@{
    RunId = $runId; Started = $started.ToString('o'); Finished = [DateTimeOffset]::Now.ToString('o')
    NativeExitCode = $nativeExitCode; WrapperExitCode = $wrapperExitCode
    BuildResult = $buildResult; ElapsedSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
    LaunchError = $launchError; Project = $project; EvidenceDirectory = $out; UBTLog = $ubtLog
    BinariesExistedBeforeBuild = $binariesBefore; IntermediateExistedBeforeBuild = $intermediateBefore
    ToolchainValidation = $toolchainStatus; SelectedToolchain = $selectedToolchain; ToolchainReadError = $toolchainReadError
    EditorPIE = 'NOT RUN'; GPUStability = 'NOT RUN'
}
Write-Json (Join-Path $out 'build-result.json') $result
$result | ConvertTo-Json -Depth 12
exit $wrapperExitCode

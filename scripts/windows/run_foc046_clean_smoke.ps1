<#
.SYNOPSIS
Builds a new disposable Blank C++ editor target with an installed Unreal Engine.
.DESCRIPTION
Copies only six text files from the installed TP_Blank template. Each run creates
new directories outside any Git checkout. No delete, engine edit, installation,
global configuration change, Editor launch, or PIE launch is performed.
Raw evidence contains local paths: keep it outside Git and publish a reviewed,
sanitized summary only. A new project is clean; engine/shared caches are not reset.
.PARAMETER WorkspaceRoot
Optional NEW directory for this run. Defaults to a unique directory under TEMP.
.PARAMETER EvidenceRoot
Local directory outside all Git checkouts. A unique run directory is created here.
.PARAMETER CompilerVersion
Optional numeric MSVC version passed through the installed UBT command-line API.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EngineRoot,
    [string]$WorkspaceRoot,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EvidenceRoot,
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$CompilerVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-LocalFullPath([string]$Value) {
    # Avoid network output and cmd.exe expansion/injection. Parentheses are safe
    # inside the quoted argument list; the other cmd metacharacters are rejected.
    if ($Value -notmatch '^[A-Za-z]:[\\/]' -or $Value -match '["%\x21&|<>^\r\n]') {
        throw 'Use an absolute local drive path without cmd metacharacters.'
    }
    $fullPath = [IO.Path]::GetFullPath($Value)
    # Reject junctions/symlinks rather than risk an output escaping the checks.
    for ($current = $fullPath; $current; $current = [IO.Path]::GetDirectoryName($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Reparse-point paths are not supported by this smoke script.'
            }
        }
    }
    if ($fullPath -eq [IO.Path]::GetPathRoot($fullPath)) { return $fullPath }
    return $fullPath.TrimEnd([char[]]'\/')
}

function Assert-OutsideGit([string]$Path) {
    for ($current = $Path; $current; $current = [IO.Path]::GetDirectoryName($current)) {
        if (Test-Path -LiteralPath (Join-Path $current '.git')) {
            throw 'Disposable workspace and raw evidence must be outside every Git checkout.'
        }
    }
}

function Test-IsWithin([string]$Path, [string]$Parent) {
    return $Path.Equals($Parent, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Parent.TrimEnd([char[]]'\/') + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}

$runId = 'FOC046-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
if (!$WorkspaceRoot) { $WorkspaceRoot = Join-Path ([IO.Path]::GetTempPath()) ($runId + '-workspace') }
$EngineRoot = Get-LocalFullPath $EngineRoot
$WorkspaceRoot = Get-LocalFullPath $WorkspaceRoot
$EvidenceRoot = Get-LocalFullPath $EvidenceRoot
$out = Join-Path $EvidenceRoot $runId
$projectDir = Join-Path $WorkspaceRoot 'Foc046Smoke'
$project = Join-Path $projectDir 'Foc046Smoke.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$template = Join-Path $EngineRoot 'Templates\TP_Blank'
$versionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'

foreach ($path in @($WorkspaceRoot, $EvidenceRoot)) {
    Assert-OutsideGit $path
    if (Test-IsWithin $path $EngineRoot) { throw 'Output inside the engine installation is forbidden.' }
}
if ((Test-IsWithin $out $WorkspaceRoot) -or (Test-IsWithin $WorkspaceRoot $out)) {
    throw 'Workspace and evidence run directories must be separate.'
}
foreach ($path in @($WorkspaceRoot, $out)) {
    if (Test-Path -LiteralPath $path) { throw 'Refusing an existing run directory; choose a new workspace.' }
}
foreach ($path in @($build, $versionFile)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw 'Required installed engine file is missing.' }
}
if ($CompilerVersion) {
    $windowsRules = Join-Path $EngineRoot 'Engine\Source\Programs\UnrealBuildTool\Platform\Windows\UEBuildWindows.cs'
    if (!(Test-Path -LiteralPath $windowsRules -PathType Leaf) -or
        !([IO.File]::ReadAllText($windowsRules).Contains('[CommandLine("-CompilerVersion")]'))) {
        throw 'Could not verify -CompilerVersion support in this installed engine.'
    }
}
$engineVersion = [IO.File]::ReadAllText($versionFile) | ConvertFrom-Json
$files = @(
    'TP_Blank.uproject',
    'Source\TP_Blank.Target.cs',
    'Source\TP_BlankEditor.Target.cs',
    'Source\TP_Blank\TP_Blank.Build.cs',
    'Source\TP_Blank\TP_Blank.cpp',
    'Source\TP_Blank\TP_Blank.h'
)
# Read/validate everything before creating output directories. Preserve template
# build/include settings (including V7/Unreal5_8 in the UE 5.8 template).
$contentsByFile = @{}
foreach ($relative in $files) {
    $source = Join-Path $template $relative
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw 'Required TP_Blank source template is missing.' }
    $contents = [IO.File]::ReadAllText($source).Replace('TP_Blank', 'Foc046Smoke')
    if ($relative.EndsWith('.uproject')) {
        $descriptor = $contents | ConvertFrom-Json
        $descriptor.EngineAssociation = "$($engineVersion.MajorVersion).$($engineVersion.MinorVersion)"
        $contents = $descriptor | ConvertTo-Json -Depth 12
    }
    $contentsByFile[$relative] = $contents
}
$arguments = @(
    'Foc046SmokeEditor', 'Win64', 'Development', "-Project=$project",
    '-WaitMutex', '-NoHotReloadFromIDE', '-NoHotReload', '-MaxParallelActions=4',
    "-Log=$(Join-Path $out 'UnrealBuildTool.log')"
)
if ($CompilerVersion) { $arguments += "-CompilerVersion=$CompilerVersion" }
foreach ($value in @($build) + $arguments) {
    if ($value -match '["%\x21&|<>^\r\n]') { throw 'Unsafe character in build argument.' }
}
New-Item -ItemType Directory -Path (Join-Path $projectDir 'Source\Foc046Smoke'), $out | Out-Null
foreach ($relative in $files) {
    $destination = Join-Path $projectDir ($relative.Replace('TP_Blank', 'Foc046Smoke'))
    [IO.File]::WriteAllText($destination, $contentsByFile[$relative], [Text.UTF8Encoding]::new($false))
}
$before = [ordered]@{
    RunId = $runId
    Project = $project
    EngineRoot = $EngineRoot
    EvidenceDirectory = $out
    EngineBuild = $engineVersion
    RequestedCompilerVersion = $CompilerVersion
    ProjectCreation = 'Six text files instantiated from installed TP_Blank; not created through Editor UI'
    SourceFiles = $files
    BinariesExistedBeforeBuild = Test-Path -LiteralPath (Join-Path $projectDir 'Binaries')
    IntermediateExistedBeforeBuild = Test-Path -LiteralPath (Join-Path $projectDir 'Intermediate')
    EngineRebuild = $false
    DDCState = 'Existing engine/shared caches are uncontrolled and are not cleared'
}
Write-Json (Join-Path $out 'pre-build.json') $before
Write-Json (Join-Path $out 'command.json') @{ Executable = $build; Arguments = $arguments }
# No shell environment changes: run the installed batch file via quoted cmd.exe.
$commandLine = '""' + $build + '" ' + (($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ') + '"'
$started = [DateTimeOffset]::Now
$timer = [Diagnostics.Stopwatch]::StartNew()
$exitCode = $null
$launchError = $null
try {
    $process = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d', '/s', '/c', $commandLine) `
        -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $out 'stdout.txt') `
        -RedirectStandardError (Join-Path $out 'stderr.txt')
    $exitCode = $process.ExitCode
}
catch { $launchError = $_.Exception.Message }
finally { $timer.Stop() }
$result = [ordered]@{
    RunId = $runId
    Started = $started.ToString('o')
    Finished = [DateTimeOffset]::Now.ToString('o')
    ExitCode = $exitCode
    ElapsedSeconds = [math]::Round($timer.Elapsed.TotalSeconds, 3)
    Result = $(if ($null -eq $exitCode) { 'NOT RUN' } elseif ($exitCode -eq 0) { 'PASS' } else { 'FAIL' })
    LaunchError = $launchError
    Project = $project
    EvidenceDirectory = $out
    UBTLog = Join-Path $out 'UnrealBuildTool.log'
    EditorPIE = 'NOT RUN'
    GPUStability = 'NOT RUN'
}
Write-Json (Join-Path $out 'build-result.json') $result
# Preserve UBT's original Unicode log and both native streams without redaction or
# re-encoding. These local files are evidence, never publication-ready artifacts.
$result | ConvertTo-Json -Depth 12
if ($null -eq $exitCode) { exit 1 }
exit $exitCode

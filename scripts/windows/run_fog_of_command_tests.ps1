<#
.SYNOPSIS
Runs FogOfCommand Smoke, UnitState and Orders tests in installed UE 5.8.4.
.DESCRIPTION
Build FogOfCommandEditor first. This is unattended NullRHI asset/config/module
validation, not GUI/PIE, rendering, packaging or an Engine build. No locale override.
Every run uses a new external evidence directory; raw logs/reports may contain
private paths and must not be committed/uploaded. No output/cache deletion.
An exit code of zero alone never passes: require the complete successful suite
in a freshly exported index.json. Warnings fail the project suite as well.
The default All suite requires all 17 tests. Optional Smoke/UnitState/Orders
subsets require their exact three/six/eight tests and cannot replace the combined gate.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..\..'),
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [ValidateRange(30, 3600)][int]$TimeoutSeconds = 600,
    [ValidateSet('All', 'Smoke', 'UnitState', 'Orders')][string]$Suite = 'All'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'automation_report.ps1')

function Get-LocalPath([string]$Value) {
    if ($Value -notmatch '^[A-Za-z]:[\\/]' -or $Value -match '["%\x21&|<>^\r\n]') {
        throw 'Use an absolute local drive path without command metacharacters.'
    }
    $full = [IO.Path]::GetFullPath($Value)
    for ($current = $full; $current; $current = [IO.Path]::GetDirectoryName($current)) {
        if ((Test-Path -LiteralPath $current) -and
            ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Paths through junctions/symlinks are not supported.'
        }
    }
    if ($full -eq [IO.Path]::GetPathRoot($full)) { return $full }
    return $full.TrimEnd([char[]]'\/')
}
function Test-Within([string]$Path, [string]$Parent) {
    return $Path.Equals($Parent, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Parent.TrimEnd([char[]]'\/') + '\', [StringComparison]::OrdinalIgnoreCase)
}
function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}
$EngineRoot = Get-LocalPath $EngineRoot
$ProjectRoot = Get-LocalPath $ProjectRoot
$EvidenceRoot = Get-LocalPath $EvidenceRoot
for ($current = $EvidenceRoot; $current; $current = [IO.Path]::GetDirectoryName($current)) {
    if (Test-Path -LiteralPath (Join-Path $current '.git')) { throw 'Evidence must be outside all Git checkouts.' }
}
if ((Test-Within $EvidenceRoot $EngineRoot) -or (Test-Within $EvidenceRoot $ProjectRoot) -or
    (Test-Within $ProjectRoot $EngineRoot)) { throw 'Project/evidence must not be inside the engine; evidence must be outside the project.' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$versionPath = Join-Path $EngineRoot 'Engine\Build\Build.version'
$project = Join-Path $ProjectRoot 'FogOfCommand.uproject'
$module = Join-Path $ProjectRoot 'Binaries\Win64\UnrealEditor-FogOfCommand.dll'
foreach ($required in @($editor, $versionPath, $project, $module)) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw 'Required UE/project/built Editor module missing. Build FogOfCommandEditor first.' }
}
$version = [IO.File]::ReadAllText($versionPath) | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 4 -or
    $version.Changelist -ne 58902052) { throw 'Expected UE 5.8.4 CL58902052; no test process launched.' }
$descriptor = [IO.File]::ReadAllText($project) | ConvertFrom-Json
if ($descriptor.EngineAssociation -ne '5.8' -or 'FogOfCommand' -notin @($descriptor.Modules | ForEach-Object { $_.Name })) {
    throw 'Expected FogOfCommand project associated with UE 5.8.'
}
$runId = 'FOC003-tests-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$out = Join-Path $EvidenceRoot $runId
if (Test-Path -LiteralPath $out) { throw 'Refusing to reuse evidence or a stale report.' }
New-Item -ItemType Directory -Path $out | Out-Null
$reportDir = Join-Path $out 'AutomationReport'
New-Item -ItemType Directory -Path $reportDir | Out-Null
$reportFile = Join-Path $reportDir 'index.json'
$log = Join-Path $out 'UnrealEditor.log'
$expectedTests = @(Get-FocExpectedTests -Suite $Suite)
$testPrefix = switch ($Suite) {
    'Smoke' { 'FogOfCommand.Smoke.' }
    'UnitState' { 'FogOfCommand.UnitState.' }
    'Orders' { 'FogOfCommand.Orders.' }
    'All' { 'FogOfCommand.' }
}
$arguments = @($project, '-unattended', '-nop4', '-nosplash', '-nullrhi', '-stdout', '-FullStdOutLogOutput',
    '-LogCmds=LogAutomationTest Log',
    '-TestLoops=1', "-ExecCmds=Automation RunTests StartsWith:$testPrefix;Quit",
    "-ReportExportPath=$reportDir", "-abslog=$log")
Write-Json (Join-Path $out 'command.json') @{ Executable = $editor; Arguments = $arguments }
$started = [DateTimeOffset]::Now
$timer = [Diagnostics.Stopwatch]::StartNew()
$nativeExitCode = $null
$timedOut = $false
$launchError = $null
$processLaunched = $false
try {
    $quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $process = Start-Process -FilePath $editor -ArgumentList $quoted -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $out 'stdout.txt') -RedirectStandardError (Join-Path $out 'stderr.txt')
    $processLaunched = $true
    # Windows PowerShell 5.1 can otherwise lose ExitCode after an asynchronous
    # Start-Process with redirected output. Open/cache the owned handle while
    # the child is alive; do not infer the native code from an Editor log marker.
    $ownedHandle = $process.Handle
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        $timedOut = $true
        # Only the unattended process created by this invocation is terminated.
        $process.Kill()
        if (!$process.WaitForExit(10000)) { throw 'Timed-out process did not terminate within the 10-second cleanup limit.' }
    }
    $process.Refresh()
    $nativeExitCode = $process.ExitCode
} catch { $launchError = $_.Exception.Message }
finally { $timer.Stop() }

$verdict = [pscustomobject]@{ Status = 'FAIL'; Problems = @('Missing freshly exported AutomationReport/index.json.') }
try {
    if (Test-Path -LiteralPath $reportFile -PathType Leaf) {
        $report = [IO.File]::ReadAllText($reportFile) | ConvertFrom-Json
        $verdict = Test-FocAutomationReport -Report $report -Suite $Suite
    }
} catch { $verdict = [pscustomobject]@{ Status = 'FAIL'; Problems = @('Malformed or unreadable Automation report.') } }
$pass = !$timedOut -and !$launchError -and $null -ne $nativeExitCode -and $nativeExitCode -eq 0 -and $verdict.Status -eq 'PASS'
$result = [ordered]@{
    RunId = $runId; Started = $started.ToString('o'); Finished = [DateTimeOffset]::Now.ToString('o')
    NativeExitCode = $nativeExitCode; WrapperExitCode = $(if ($pass) { 0 } else { 1 })
    Status = $(if ($pass) { 'PASS' } elseif (!$processLaunched) { 'NOT RUN' } else { 'FAIL' })
    ElapsedSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3); TimeoutSeconds = $TimeoutSeconds
    TimedOut = $timedOut; LaunchError = $launchError
    ReportValidation = $verdict; ReportFile = $reportFile; EditorLog = $log; Project = $project; EngineBuild = $version
    Suite = $Suite; TestPrefix = $testPrefix; ExpectedTests = $expectedTests
    Mode = 'Unattended NullRHI; no culture override'
    EngineStartupDiagnostics = 'Separate from project suite. Inspect raw log; known ru-RU failures tracked in #55 are not suppressed.'
    EditorGUI = 'NOT RUN'; PIE = 'NOT RUN'; GPURendering = 'NOT RUN'; EngineBuildPerformed = $false
}
Write-Json (Join-Path $out 'test-result.json') $result
$result | ConvertTo-Json -Depth 12
exit $result.WrapperExitCode

<#
.SYNOPSIS
Checks FOC-006 isolated developer commands with opt-in disabled and enabled.
.DESCRIPTION
Requires a built Development Editor module and installed UE 5.8.4 CL58902052.
Loads Startup with NullRHI; does not exercise GUI/PIE, rendering or Shipping.
Runs only fixed no-argument commands plus a fixed harmless invalid argument.
Evidence contains raw local paths/logs and must remain outside every checkout.
This helper validates scenario markers, ordered domain records and opt-in; inspect
all other Editor diagnostics separately, including known Engine startup issue #55.
No installs, locale overrides, output/cache deletion or other-process termination.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..\..'),
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [ValidateRange(30, 3600)][int]$TimeoutSeconds = 180
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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
    $null = Get-LocalPath $required
}
$version = [IO.File]::ReadAllText($versionPath) | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 4 -or
    $version.Changelist -ne 58902052) { throw 'Expected UE 5.8.4 CL58902052; no debug process launched.' }
$descriptor = [IO.File]::ReadAllText($project) | ConvertFrom-Json
if ($descriptor.EngineAssociation -ne '5.8' -or 'FogOfCommand' -notin @($descriptor.Modules | ForEach-Object { $_.Name })) {
    throw 'Expected FogOfCommand project associated with UE 5.8.'
}
$runId = 'FOC006-debug-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$out = Join-Path $EvidenceRoot $runId
if (Test-Path -LiteralPath $out) { throw 'Refusing to overwrite or reuse debug evidence.' }
New-Item -ItemType Directory -Path $out | Out-Null

# Ordinary ExecCmds uses commas; semicolon is internal to the Automation queue.
$cases = @(
    @{ Name = 'disabled'; Enabled = $false; Commands = 'foc.EventLog.RunScenario,foc.EventLog.Dump,Quit' },
    @{ Name = 'enabled'; Enabled = $true; Commands = 'foc.EventLog.Dump,foc.EventLog.RunScenario rejected,foc.EventLog.RunScenario,foc.EventLog.Dump,foc.EventLog.RunScenario rejected,foc.EventLog.Dump,Quit' }
)
$expected = @(
    @{ Type = 'UnitCreated'; Tick = 0; Reason = 'Created' },
    @{ Type = 'UnitMoved'; Tick = 1; Reason = 'PositionChanged' },
    @{ Type = 'AmmunitionConsumed'; Tick = 2; Reason = 'AmmunitionSpent' },
    @{ Type = 'UnitDamaged'; Tick = 3; Reason = 'DamageApplied' },
    @{ Type = 'OrderSubmitted'; Tick = 4; Reason = 'None' },
    @{ Type = 'OrderStarted'; Tick = 5; Reason = 'None' },
    @{ Type = 'OrderCompleted'; Tick = 6; Reason = 'Completed' },
    @{ Type = 'OrderSubmitted'; Tick = 7; Reason = 'None' },
    @{ Type = 'OrderStarted'; Tick = 8; Reason = 'None' },
    @{ Type = 'OrderCancelled'; Tick = 9; Reason = 'Replaced' },
    @{ Type = 'OrderSubmitted'; Tick = 9; Reason = 'None' },
    @{ Type = 'OrderReplaced'; Tick = 9; Reason = 'Replaced' },
    @{ Type = 'OrderStarted'; Tick = 10; Reason = 'None' },
    @{ Type = 'OrderFailed'; Tick = 11; Reason = 'ExecutionFailed' },
    @{ Type = 'OrderSubmitted'; Tick = 12; Reason = 'None' },
    @{ Type = 'OrderCancelled'; Tick = 13; Reason = 'CancelledByRequest' },
    @{ Type = 'OrderSubmitted'; Tick = 14; Reason = 'None' },
    @{ Type = 'OrderStarted'; Tick = 15; Reason = 'None' },
    @{ Type = 'UnitDamaged'; Tick = 16; Reason = 'DamageApplied' },
    @{ Type = 'OrderCancelled'; Tick = 16; Reason = 'UnitDestroyed' },
    @{ Type = 'UnitDestroyed'; Tick = 16; Reason = 'DamageApplied' }
)
$results = @()
foreach ($case in $cases) {
    $log = Join-Path $out ($case.Name + '.log')
    $arguments = @($project, '/Game/Maps/Startup', '-unattended', '-nop4', '-nosplash', '-nullrhi', '-stdout',
        '-FullStdOutLogOutput', '-LogCmds=LogAutomationTest Log', ('-ExecCmds=' + $case.Commands), ('-abslog=' + $log))
    if ($case.Enabled) { $arguments += '-FocEventDebug' }
    Write-Json (Join-Path $out ($case.Name + '-command.json')) @{ Executable = $editor; Arguments = $arguments }
    $started = [DateTimeOffset]::Now
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = $null
    $nativeExitCode = $null
    $timedOut = $false
    $launchError = $null
    $cleanupError = $null
    try {
        $quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $process = Start-Process -FilePath $editor -ArgumentList $quoted -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $out ($case.Name + '-stdout.txt')) `
            -RedirectStandardError (Join-Path $out ($case.Name + '-stderr.txt'))
        # Cache the owned handle while alive, preserving native ExitCode on PS5.1.
        $ownedHandle = $process.Handle
        if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
            $timedOut = $true
            $process.Kill()
            if (!$process.WaitForExit(10000)) { throw 'Owned process did not exit within the cleanup limit.' }
        }
        $process.Refresh()
        $nativeExitCode = $process.ExitCode
    } catch { $launchError = $_.Exception.Message }
    finally {
        if ($null -ne $process) {
            try {
                if (!$process.HasExited) {
                    $process.Kill()
                    if (!$process.WaitForExit(10000)) { throw 'Owned process remained alive after cleanup.' }
                }
            } catch { $cleanupError = $_.Exception.Message }
            finally { $process.Dispose() }
        }
        $timer.Stop()
    }
    $problems = [Collections.Generic.List[string]]::new()
    if ($timedOut) { $problems.Add('Owned process timed out.') }
    if ($launchError -or $cleanupError -or $null -eq $nativeExitCode -or $nativeExitCode -ne 0) {
        $problems.Add('Native execution did not complete cleanly with exit 0.')
    }
    $lines = @()
    try { $lines = @([IO.File]::ReadAllLines($log)) }
    catch { $problems.Add('Fresh Editor log is missing or unreadable.') }
    $events = @($lines | Where-Object { $_ -cmatch 'FOC_EVENT seq=' } | ForEach-Object { $_.Substring($_.IndexOf('FOC_EVENT seq=', [StringComparison]::Ordinal)) })
    $scenario = @($lines | Where-Object { $_ -cmatch 'FOC_EVENT_DEBUG PASS scenario:' })
    $rejected = @($lines | Where-Object { $_ -cmatch 'FOC_EVENT_DEBUG REJECTED:' })
    $empty = @($lines | Where-Object { $_ -cmatch 'FOC_EVENT_DEBUG EMPTY:' })
    $dump = @($lines | Where-Object { $_ -cmatch 'FOC_EVENT_DEBUG DUMP retained=21 total=21 evicted=0' })
    $mapLoaded = @($lines | Where-Object { $_ -cmatch 'LogLoad:.*LoadMap: /Game/Maps/Startup' })
    $unknown = @($lines | Where-Object { $_ -cmatch 'Command not recognized: foc\.EventLog\.' })
    $failed = @($lines | Where-Object { $_ -cmatch 'FOC_EVENT_DEBUG FAIL:' })
    if ($mapLoaded.Count -lt 1) { $problems.Add('Startup map load was not observed.') }
    if ($failed.Count) { $problems.Add('Isolated scenario reported failure.') }
    $sequenceOk = $true
    $sameDump = $true
    $types = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $unitIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    if ($case.Enabled) {
        if ($events.Count -ne 42 -or $scenario.Count -ne 1 -or $rejected.Count -ne 2 -or
            $empty.Count -ne 1 -or $dump.Count -ne 2 -or $unknown.Count -ne 0) {
            $problems.Add('Enabled scenario/empty/refusal/double-dump markers do not match the contract.')
        }
        for ($index = 0; $index -lt $events.Count; $index++) {
            $match = [regex]::Match($events[$index], '^FOC_EVENT seq=(\d+) tick=(\d+) type=(\w+) unit=([0-9A-Fa-f-]+) order=(\S+) related=(\S+) reason=(\w+) status=(\w+)->(\w+)\s*$')
            if (!$match.Success) { $sequenceOk = $false; continue }
            $reference = $expected[$index % 21]
            if ($match.Groups[1].Value -cne [string](($index % 21) + 1) -or
                $match.Groups[2].Value -cne [string]$reference.Tick -or
                $match.Groups[3].Value -cne $reference.Type -or $match.Groups[7].Value -cne $reference.Reason) {
                $sequenceOk = $false
            }
            $parsedId = [guid]::Empty
            if (![guid]::TryParse($match.Groups[4].Value, [ref]$parsedId) -or $parsedId -eq [guid]::Empty) { $sequenceOk = $false }
            $null = $unitIds.Add($match.Groups[4].Value)
            $null = $types.Add($match.Groups[3].Value)
            if ($index -ge 21 -and $events[$index] -cne $events[$index - 21]) { $sameDump = $false }
        }
        if (!$sequenceOk -or $types.Count -ne 11 -or $unitIds.Count -ne 1 -or !$sameDump) {
            $problems.Add('Ordered ticks/types/reasons/identity or unchanged repeated dump failed.')
        }
    } elseif ($events.Count -ne 0 -or $scenario.Count -ne 0 -or $unknown.Count -ne 2 -or
        $rejected.Count -ne 0 -or $empty.Count -ne 0 -or $dump.Count -ne 0) {
        $problems.Add('Commands must be unregistered and emit no scenario/domain records without opt-in.')
    }
    $results += [pscustomobject]@{
        Case = $case.Name; Status = $(if (!$problems.Count) { 'PASS' } elseif ($null -eq $process -and $null -eq $nativeExitCode) { 'NOT RUN' } else { 'FAIL' })
        NativeExitCode = $nativeExitCode; Started = $started.ToString('o'); Finished = [DateTimeOffset]::Now.ToString('o')
        ElapsedSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3); TimeoutSeconds = $TimeoutSeconds; TimedOut = $timedOut
        LaunchError = $launchError; CleanupError = $cleanupError; Problems = @($problems.ToArray())
        MapLoads = $mapLoaded.Count; Events = $events.Count; Scenarios = $scenario.Count; Rejected = $rejected.Count
        Empty = $empty.Count; Dumps = $dump.Count; UnknownCommands = $unknown.Count; SequenceOK = $sequenceOk
        IdenticalDumps = $sameDump; Kinds = $types.Count; UnitIdentities = $unitIds.Count
    }
}
$pass = @($results | Where-Object { $_.Status -ne 'PASS' }).Count -eq 0
$result = [ordered]@{
    RunId = $runId; Status = $(if ($pass) { 'PASS' } else { 'FAIL' }); WrapperExitCode = $(if ($pass) { 0 } else { 1 })
    Cases = $results; EngineBuild = $version; Project = $project; EvidenceDirectory = $out
    FullEditorLogAudit = 'NOT RUN by this helper; inspect other diagnostics separately, including Engine startup issue #55.'
    Mode = 'Unattended Startup map / NullRHI; isolated developer domain values; no culture override'
    EditorGUI = 'NOT RUN'; PIE = 'NOT RUN'; GPURendering = 'NOT RUN'; ShippingBuild = 'NOT RUN'
}
Write-Json (Join-Path $out 'debug-result.json') $result
# Keep raw paths, exceptions, command line and Editor/stdout/stderr content local.
[pscustomobject]@{ RunId = $runId; Status = $result.Status; WrapperExitCode = $result.WrapperExitCode
    Cases = @($results | Select-Object Case, Status, NativeExitCode, ElapsedSeconds, MapLoads, Events, Scenarios,
        Rejected, Empty, Dumps, UnknownCommands, SequenceOK, IdenticalDumps, Kinds, UnitIdentities)
} | ConvertTo-Json -Depth 6
exit $result.WrapperExitCode

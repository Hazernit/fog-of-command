<#
.SYNOPSIS
Collects bounded, read-only GPU/process/log observations for a running Editor.
.DESCRIPTION
Does not launch or control Editor/PIE, generate GPU load, install components, or
change settings. EvidenceRoot must be outside every Git checkout. Raw evidence
may contain private paths/messages; publish only a reviewed, sanitized report.
Telemetry does not prove PIE remained active or establish general GPU stability.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 2147483647)]
    [int]$EditorProcessId,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EditorLog,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EvidenceRoot,
    [ValidateRange(1, 7200)]
    [int]$DurationSeconds = 1800,
    [ValidateRange(1, 60)]
    [int]$IntervalSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-LocalPath([string]$Value) {
    if ($Value -notmatch '^[A-Za-z]:[\\/]' -or $Value -match '[\r\n]') {
        throw 'Use an absolute local drive path.'
    }
    return [IO.Path]::GetFullPath($Value)
}

function Assert-OutsideGit([string]$Path) {
    for ($current = $Path; $current; $current = [IO.Path]::GetDirectoryName($current)) {
        if (Test-Path -LiteralPath $current) {
            if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Evidence paths through junctions/symlinks are not supported.'
            }
        }
        if (Test-Path -LiteralPath (Join-Path $current '.git')) {
            throw 'Raw GPU telemetry must be outside every Git checkout.'
        }
    }
}

function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}

function Read-SystemEvents([DateTimeOffset]$From, [DateTimeOffset]$To) {
    try {
        $all = @(Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = $From.LocalDateTime; EndTime = $To.LocalDateTime } -ErrorAction Stop)
        $selected = @($all | Where-Object {
            $_.ProviderName -in @('Display', 'nvlddmkm', 'Microsoft-Windows-WHEA-Logger') -or
            ($_.ProviderName -eq 'Microsoft-Windows-Kernel-Power' -and $_.Id -eq 41) -or
            ($_.ProviderName -eq 'EventLog' -and $_.Id -eq 6008)
        })
        return [ordered]@{
            Status = 'PASS'; WindowStart = $From.ToString('o'); WindowEnd = $To.ToString('o')
            QueriedSystemEventCount = $all.Count; SelectedEventCount = $selected.Count
            Events = @($selected | ForEach-Object {
                [ordered]@{ TimeCreated = $_.TimeCreated.ToString('o'); Id = $_.Id; Provider = $_.ProviderName; RecordId = $_.RecordId; Message = $_.Message; Xml = $_.ToXml() }
            })
            Error = $null
        }
    } catch {
        $noEvents = $_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*'
        return [ordered]@{
            Status = $(if ($noEvents) { 'PASS' } else { 'NOT RUN - System log unavailable; no administrator access requested' })
            WindowStart = $From.ToString('o'); WindowEnd = $To.ToString('o')
            QueriedSystemEventCount = $(if ($noEvents) { 0 } else { $null })
            SelectedEventCount = $(if ($noEvents) { 0 } else { $null })
            Events = @(); Error = $(if ($noEvents) { $null } else { $_.Exception.Message })
        }
    }
}

function Invoke-GpuQuery([string]$Executable, [int]$TimeoutMilliseconds) {
    if (!$Executable) { return @{ Status = 'NOT RUN - nvidia-smi unavailable'; ExitCode = $null; Raw = ''; Error = $null } }
    $child = [Diagnostics.Process]::new()
    try {
        $child.StartInfo = [Diagnostics.ProcessStartInfo]::new()
        $child.StartInfo.FileName = $Executable
        $child.StartInfo.Arguments = '--query-gpu=timestamp,name,driver_version,temperature.gpu,utilization.gpu,memory.used,memory.total,power.draw --format=csv,noheader,nounits'
        $child.StartInfo.UseShellExecute = $false
        $child.StartInfo.CreateNoWindow = $true
        $child.StartInfo.RedirectStandardOutput = $true
        $child.StartInfo.RedirectStandardError = $true
        [void]$child.Start()
        $stdout = $child.StandardOutput.ReadToEndAsync()
        $stderr = $child.StandardError.ReadToEndAsync()
        if (!$child.WaitForExit($TimeoutMilliseconds)) {
            # Only terminate this collector's own timed-out nvidia-smi query.
            $child.Kill()
            [void]$child.WaitForExit(1000)
            return @{ Status = 'TIMEOUT'; ExitCode = $null; Raw = ''; Error = 'nvidia-smi query exceeded its bounded timeout' }
        }
        return @{ Status = $(if ($child.ExitCode -eq 0) { 'PASS' } else { 'FAIL' }); ExitCode = $child.ExitCode; Raw = $stdout.GetAwaiter().GetResult(); Error = $stderr.GetAwaiter().GetResult() }
    } catch {
        return @{ Status = 'FAIL'; ExitCode = $null; Raw = ''; Error = $_.Exception.Message }
    } finally { $child.Dispose() }
}

function Convert-Metric([string]$Value) {
    $number = 0.0
    if ([double]::TryParse($Value.Trim(), [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$number)) { return $number }
    return $null
}

function Get-MetricRange($Rows, [string]$Property) {
    $values = @($Rows | ForEach-Object { $_.$Property } | Where-Object { $null -ne $_ })
    if (!$values.Count) { return @{ Samples = 0; Minimum = $null; Maximum = $null } }
    $range = $values | Measure-Object -Minimum -Maximum
    return @{ Samples = $values.Count; Minimum = $range.Minimum; Maximum = $range.Maximum }
}

$EditorLog = Get-LocalPath $EditorLog
$EvidenceRoot = Get-LocalPath $EvidenceRoot
Assert-OutsideGit $EvidenceRoot
$out = Join-Path $EvidenceRoot ('FOC046-GPU-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
if (Test-Path -LiteralPath $out) { throw 'Refusing to overwrite existing telemetry.' }
New-Item -ItemType Directory -Path $out -Force | Out-Null
Assert-OutsideGit $out
$nvidiaCommand = Get-Command nvidia-smi.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$nvidia = if ($nvidiaCommand) { $nvidiaCommand.Source } else { Join-Path $env:SystemRoot 'System32\nvidia-smi.exe' }
if (!(Test-Path -LiteralPath $nvidia -PathType Leaf)) { $nvidia = $null }
$initialProcess = Get-Process -Id $EditorProcessId -ErrorAction SilentlyContinue
$initialProcessError = $null
$processStartTicks = $null
if ($initialProcess) {
    try { $processStartTicks = $initialProcess.StartTime.ToUniversalTime().Ticks }
    catch { $initialProcessError = $_.Exception.Message }
}
$logOffset = if (Test-Path -LiteralPath $EditorLog -PathType Leaf) { (Get-Item -LiteralPath $EditorLog).Length } else { 0L }
$initialLogOffset = $logOffset
$logPending = ''
$logPattern = '(?i)\bfatal error\b|DXGI_ERROR_DEVICE_(REMOVED|HUNG|RESET)|GPU crash detected|GPU crashed|GPU hang|GPU hung|D3D device.*(lost|removed)|LogD3D12RHI:\s*Error'
$fatalLines = [Collections.Generic.List[string]]::new()
$logErrors = [Collections.Generic.List[string]]::new()
$rows = [Collections.Generic.List[object]]::new()
$queryErrors = [Collections.Generic.List[string]]::new()
$processErrors = [Collections.Generic.List[string]]::new()
$sampleCount = 0
$logReads = 0
$logResets = 0
$nonRespondingSamples = 0
$endReason = 'DurationCompleted'
$collectorError = $null
$baselineCutoff = [DateTimeOffset]::Now
$baseline = Read-SystemEvents $baselineCutoff.AddMinutes(-15) $baselineCutoff
Write-Json (Join-Path $out 'system-before.json') $baseline
$started = [DateTimeOffset]::Now
$watch = [Diagnostics.Stopwatch]::StartNew()
Write-Json (Join-Path $out 'start.json') ([ordered]@{
    Started = $started.ToString('o'); EditorProcessId = $EditorProcessId; EditorLog = $EditorLog
    RequestedDurationSeconds = $DurationSeconds; IntervalSeconds = $IntervalSeconds
    NvidiaSmi = $nvidia; InitialLogOffsetBytes = $initialLogOffset
    InitialProcessAccessError = $initialProcessError
    Scope = 'Read-only observations; PIE state is not established by this collector.'
})
try {
    while ($watch.Elapsed.TotalSeconds -lt $DurationSeconds) {
        $sampleCount++
        $sampleTime = [DateTimeOffset]::Now
        $elapsed = [Math]::Round($watch.Elapsed.TotalSeconds, 3)
        $editorExited = $null
        $responding = $null
        $cpuSeconds = $null
        $processError = $null
        try {
            $editor = Get-Process -Id $EditorProcessId -ErrorAction SilentlyContinue
            if (!$editor) { $editorExited = $true }
            elseif ($null -eq $processStartTicks) {
                $processError = 'NOT RUN - process identity/start-time was not readable at collector start'
            }
            elseif ($editor.StartTime.ToUniversalTime().Ticks -ne $processStartTicks) { $editorExited = $true }
            else {
                $editor.Refresh()
                $editorExited = $editor.HasExited
                $responding = $editor.Responding
                $cpuSeconds = [Math]::Round($editor.TotalProcessorTime.TotalSeconds, 3)
                if (!$responding) { $nonRespondingSamples++ }
            }
        } catch { $processError = $_.Exception.Message }
        if ($processError) { $processErrors.Add(('Sample {0}: {1}' -f $sampleCount, $processError)) }
        try {
            $stream = [IO.File]::Open($EditorLog, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
            try {
                if ($stream.Length -lt $logOffset) { $logOffset = 0L; $logPending = ''; $logResets++ }
                [void]$stream.Seek($logOffset, [IO.SeekOrigin]::Begin)
                $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true, 4096, $true)
                try { $chunk = $reader.ReadToEnd(); $logOffset = $stream.Position } finally { $reader.Dispose() }
                $lines = ($logPending + $chunk) -split "`n", 0, 'SimpleMatch'
                $logPending = $lines[-1]
                for ($lineIndex = 0; $lineIndex -lt $lines.Count - 1; $lineIndex++) {
                    if ($lines[$lineIndex] -match $logPattern) { $fatalLines.Add($lines[$lineIndex].TrimEnd("`r")) }
                }
                $logReads++
            } finally { $stream.Dispose() }
        } catch { $logErrors.Add($_.Exception.Message) }
        $remainingMs = [Math]::Max(100, [Math]::Min(5000, [int](1000 * ($DurationSeconds - $watch.Elapsed.TotalSeconds))))
        $gpu = Invoke-GpuQuery $nvidia $remainingMs
        if ($gpu.Status -ne 'PASS') { $queryErrors.Add(('Sample {0}: {1}: {2}' -f $sampleCount, $gpu.Status, $gpu.Error)) }
        $gpuRows = @()
        if ($gpu.Status -eq 'PASS' -and $gpu.Raw.Trim()) {
            $gpuRows = @($gpu.Raw | ConvertFrom-Csv -Header 'GpuTimestamp', 'GpuName', 'Driver', 'Temperature', 'Utilization', 'MemoryUsed', 'MemoryTotal', 'Power')
        }
        if (!$gpuRows.Count) { $gpuRows = @($null) }
        $gpuIndex = 0
        foreach ($g in $gpuRows) {
            $row = [pscustomobject][ordered]@{
                Sample = $sampleCount; Timestamp = $sampleTime.ToString('o'); ElapsedSeconds = $elapsed
                EditorProcessId = $EditorProcessId; EditorHasExited = $editorExited; EditorResponding = $responding; EditorCpuSeconds = $cpuSeconds; ProcessAccessError = $processError
                GpuQueryStatus = $gpu.Status; GpuExitCode = $gpu.ExitCode; GpuIndex = $gpuIndex
                GpuName = $(if ($g) { $g.GpuName.Trim() } else { $null }); Driver = $(if ($g) { $g.Driver.Trim() } else { $null })
                TemperatureC = $(if ($g) { Convert-Metric $g.Temperature } else { $null })
                UtilizationPercent = $(if ($g) { Convert-Metric $g.Utilization } else { $null })
                MemoryUsedMiB = $(if ($g) { Convert-Metric $g.MemoryUsed } else { $null })
                MemoryTotalMiB = $(if ($g) { Convert-Metric $g.MemoryTotal } else { $null })
                PowerW = $(if ($g) { Convert-Metric $g.Power } else { $null })
                NewFatalGpuLogMatches = $fatalLines.Count; GpuRaw = $gpu.Raw.TrimEnd(); GpuError = $gpu.Error
            }
            $row | Export-Csv -LiteralPath (Join-Path $out 'samples.csv') -NoTypeInformation -Append -Encoding UTF8
            $rows.Add($row)
            $gpuIndex++
        }
        if ($editorExited) { $endReason = 'EditorExitedOrPidReused'; break }
        $remaining = $DurationSeconds - $watch.Elapsed.TotalSeconds
        if ($remaining -gt 0) { Start-Sleep -Milliseconds ([int][Math]::Min($IntervalSeconds * 1000, $remaining * 1000)) }
    }
} catch { $endReason = 'CollectorError'; $collectorError = $_.Exception.Message }
finally {
    $watch.Stop()
    $finished = [DateTimeOffset]::Now
    if ($logPending -match $logPattern) { $fatalLines.Add($logPending) }
    [IO.File]::WriteAllLines((Join-Path $out 'new-fatal-gpu-log-lines.txt'), $fatalLines, [Text.UTF8Encoding]::new($false))
    $after = Read-SystemEvents $started $finished
    Write-Json (Join-Path $out 'system-after.json') $after
    $summary = [ordered]@{
        Started = $started.ToString('o'); Finished = $finished.ToString('o')
        RequestedDurationSeconds = $DurationSeconds; ElapsedSeconds = [Math]::Round($watch.Elapsed.TotalSeconds, 3)
        IntervalSeconds = $IntervalSeconds; SampleCount = $sampleCount; CsvRowCount = $rows.Count
        LastSampleElapsedSeconds = $(if ($rows.Count) { $rows[$rows.Count - 1].ElapsedSeconds } else { $null })
        DurationCompleted = ($endReason -eq 'DurationCompleted' -and $watch.Elapsed.TotalSeconds -ge $DurationSeconds)
        EndReason = $endReason; CollectorError = $collectorError
        GpuQueryErrors = @($queryErrors); NonRespondingSamples = $nonRespondingSamples
        ProcessAccessErrors = @($processErrors)
        GpuRanges = @($rows | Group-Object GpuIndex | ForEach-Object {
            [ordered]@{ GpuIndex = $_.Name; GpuName = $_.Group[0].GpuName; TemperatureC = Get-MetricRange $_.Group 'TemperatureC'; UtilizationPercent = Get-MetricRange $_.Group 'UtilizationPercent'; MemoryUsedMiB = Get-MetricRange $_.Group 'MemoryUsedMiB'; PowerW = Get-MetricRange $_.Group 'PowerW' }
        })
        LogScan = [ordered]@{ Status = $(if ($logReads -gt 0 -and !$logErrors.Count) { 'PASS' } elseif ($logReads -gt 0) { 'PARTIAL' } else { 'NOT RUN - log unavailable' }); InitialOffsetBytes = $initialLogOffset; FinalOffsetBytes = $logOffset; SuccessfulReads = $logReads; ResetCount = $logResets; NewMatchingLines = $fatalLines.Count; Errors = @($logErrors) }
        SystemBefore = [ordered]@{ Status = $baseline.Status; SelectedEventCount = $baseline.SelectedEventCount; Interpretation = 'Historical pre-collection window; may include earlier Editor/PIE activity, not caused by the later observation window.' }
        SystemDuring = [ordered]@{ Status = $after.Status; SelectedEventCount = $after.SelectedEventCount; Interpretation = 'Time correlation only; event presence does not establish cause.' }
        NotRun = @('Verification that PIE remained active', 'Independent GPU stress test', 'General GPU/driver stability proof', 'Causal diagnosis')
        Interpretation = 'Completed collection is not an automatic PASS for Editor/PIE/GPU. Review samples, runtime evidence, log matches and event access gaps.'
    }
    Write-Json (Join-Path $out 'summary.json') $summary
    [pscustomobject]@{ EvidenceDirectory = $out; Summary = $summary } | ConvertTo-Json -Depth 12
}
if ($collectorError -or $endReason -ne 'DurationCompleted') { exit 1 }

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\windows\event_debug_report.ps1')
$cases = @(
    @{ Lines = @(); Expected = @() },
    @{ Lines = @('[2026.10.27-11.56.05:211][  0]foc.EventLog.Dump',
        '[2026.10.27-11.56.05:211][  0]foc.EventLog.RunScenario'); Expected = @('foc.EventLog.Dump', 'foc.EventLog.RunScenario') },
    @{ Lines = @('foc.EventLog.Dump'); Expected = @('foc.EventLog.Dump') },
    @{ Lines = @('[2026.10.27-11.56.05:211][  0]LogConsoleResponse: Display: foc.EventLog.Dump'); Expected = @('foc.EventLog.Dump') },
    @{ Lines = @('[2026.10.27-11.56.05:211][  0]Cmd: foc.EventLog.Dump',
        '[2026.10.27-11.56.05:211][  0]Cmd: DumpCCmds foc.EventLog.',
        'LogInit: Command Line: foc.EventLog.Dump',
        'LogCsvProfiler: commandline=foc.EventLog.Dump'); Expected = @() },
    @{ Lines = @('FOC_EVENT_DEBUG EMPTY: run foc.EventLog.RunScenario first.',
        "HELP for 'foc.EventLog.Dump':", 'foc.EventLog.Dump extra'); Expected = @() },
    @{ Lines = @('foc.EventLog.Dump', 'foc.EventLog.Dump'); Expected = @('foc.EventLog.Dump', 'foc.EventLog.Dump') },
    @{ Lines = @('foc.EventLog.UnexpectedCommand'); Expected = @('foc.EventLog.UnexpectedCommand') }
)
foreach ($case in $cases) {
    $actual = @(Get-FocRegisteredEventDebugCommands -Lines $case.Lines)
    if ($actual.Count -ne $case.Expected.Count -or
        [string]::Join('|', $actual) -cne [string]::Join('|', $case.Expected)) {
        throw 'Developer registry report regression.'
    }
}
Write-Output ('PASS: ' + $cases.Count + ' developer registry report cases (synthetic; Unreal NOT RUN).')

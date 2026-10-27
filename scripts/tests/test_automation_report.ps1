Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\windows\automation_report.ps1')

function New-GoodReport([string]$Suite = 'All') {
    $names = @(Get-FocExpectedTests -Suite $Suite)
    return [pscustomobject]@{
        succeeded = $names.Count; succeededWithWarnings = 0; failed = 0; notRun = 0; inProcess = 0
        tests = @($names | ForEach-Object {
            [pscustomobject]@{ fullTestPath = $_; state = 'Success'; warnings = 0; errors = 0 }
        })
    }
}
$script:caseCount = 0
function Assert-Status($Report, [string]$Expected, [string]$Case, [string]$Suite = 'All') {
    $verdict = Test-FocAutomationReport -Report $Report -Suite $Suite
    if ($verdict.Status -ne $Expected) { throw "Report validation regression: $Case" }
    $script:caseCount++
}
foreach ($suite in @('All', 'Smoke', 'UnitState', 'Orders', 'Events')) {
    $expectedCount = switch ($suite) { 'All' { 23 }; 'Smoke' { 3 }; 'UnitState' { 6 }; 'Orders' { 8 }; 'Events' { 6 } }
    $names = @(Get-FocExpectedTests -Suite $suite)
    if ($names.Count -ne $expectedCount -or @($names | Select-Object -Unique).Count -ne $expectedCount) {
        throw "Expected-name manifest has an incorrect count or duplicate for $suite."
    }
    Assert-Status (New-GoodReport $suite) 'PASS' "complete $suite suite" $suite
    foreach ($name in $names) {
        $report = New-GoodReport $suite
        $report.tests = @($report.tests | Where-Object { $_.fullTestPath -cne $name })
        $report.succeeded = $report.tests.Count
        Assert-Status $report 'FAIL' "missing expected $name despite internally consistent counters" $suite
    }
}
foreach ($suite in @('Smoke', 'UnitState', 'Orders', 'Events')) {
    Assert-Status (New-GoodReport $suite) 'FAIL' "$suite-only report cannot pass the default combined suite"
    Assert-Status (New-GoodReport 'All') 'FAIL' "combined suite cannot masquerade as a $suite subset" $suite
    $report = New-GoodReport
    $prefix = 'FogOfCommand.' + $suite + '.'
    $report.tests = @($report.tests | Where-Object { !$_.fullTestPath.StartsWith($prefix, [StringComparison]::Ordinal) })
    $report.succeeded = $report.tests.Count
    $case = if ($suite -eq 'Events') { 'old seventeen-test report without Events cannot pass All 23' } else { "missing entire old $suite group cannot pass All 23" }
    Assert-Status $report 'FAIL' $case
}
Assert-Status ([pscustomobject]@{}) 'FAIL' 'missing fields'
$report = New-GoodReport; $report.tests = @(); $report.succeeded = 0
Assert-Status $report 'FAIL' 'zero selected tests'
$report = New-GoodReport; $report.tests = @($report.tests[0], $report.tests[1])
Assert-Status $report 'FAIL' 'partial report with misleading success counter'
$report = New-GoodReport; $report.tests[2].state = 'Skipped'
Assert-Status $report 'FAIL' 'skipped test with misleading success counters'
$report = New-GoodReport; $report.tests[2].state = 'InProcess'
Assert-Status $report 'FAIL' 'unfinished test'
$report = New-GoodReport; $report.tests[2].state = 'Fail'; $report.tests[2].errors = 1
Assert-Status $report 'FAIL' 'failed test'
$report = New-GoodReport; $report.tests[2].fullTestPath = $report.tests[0].fullTestPath
Assert-Status $report 'FAIL' 'duplicate and missing test'
$report = New-GoodReport; $report.tests[2].fullTestPath = 'OtherProject.Smoke.Map'
Assert-Status $report 'FAIL' 'wrong suite'
$report = New-GoodReport; $report.tests[0].warnings = 1; $report.succeededWithWarnings = 1
Assert-Status $report 'FAIL' 'warnings are not a clean pass'
$report = New-GoodReport; $report.failed = 1
Assert-Status $report 'FAIL' 'inconsistent counters'
$report = New-GoodReport; $report.tests[0].PSObject.Properties.Remove('state')
Assert-Status $report 'FAIL' 'malformed test entry'
$report = New-GoodReport; $report.succeeded = $true
Assert-Status $report 'FAIL' 'boolean success counter'
$report = New-GoodReport; $report.succeeded = @(3, 3)
Assert-Status $report 'FAIL' 'array success counter'
$report = New-GoodReport; $report.tests[0].errors = @(0, 0)
Assert-Status $report 'FAIL' 'array error counter'
$report = New-GoodReport; $report.succeeded = 3.5
Assert-Status $report 'FAIL' 'fractional success counter'
$report = New-GoodReport; $report.tests[0].state = @('Success', 'Success')
Assert-Status $report 'FAIL' 'array test state'
$report = New-GoodReport; $report.tests[0].fullTestPath = @('FogOfCommand.Smoke.Configuration')
Assert-Status $report 'FAIL' 'array test name'
$report = New-GoodReport; $report.tests = $report.tests[0]
Assert-Status $report 'FAIL' 'object instead of test array'
Write-Output "PASS: $script:caseCount Automation report validator cases (synthetic; Unreal NOT RUN)."

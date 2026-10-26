Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\windows\automation_report.ps1')

function New-GoodReport {
    return ('{"succeeded":3,"succeededWithWarnings":0,"failed":0,"notRun":0,"inProcess":0,"tests":[' +
        '{"fullTestPath":"FogOfCommand.Smoke.Configuration","state":"Success","warnings":0,"errors":0},' +
        '{"fullTestPath":"FogOfCommand.Smoke.Module","state":"Success","warnings":0,"errors":0},' +
        '{"fullTestPath":"FogOfCommand.Smoke.StartupMap","state":"Success","warnings":0,"errors":0}]}') | ConvertFrom-Json
}
function Assert-Status($Report, [string]$Expected, [string]$Case) {
    $verdict = Test-FocAutomationReport $Report
    if ($verdict.Status -ne $Expected) { throw "Report validation regression: $Case" }
}
Assert-Status (New-GoodReport) 'PASS' 'complete suite'
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
Write-Output 'PASS: 19 Automation report validator cases (synthetic; Unreal NOT RUN).'

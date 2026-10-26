# Pure validation of the UE 5.8 Automation index.json contract. No UE launch or IO.
function Get-FocExpectedTests {
    [CmdletBinding()]
    param([ValidateSet('All', 'Smoke', 'UnitState')][string]$Suite = 'All')

    $smoke = @(
        'FogOfCommand.Smoke.Configuration',
        'FogOfCommand.Smoke.Module',
        'FogOfCommand.Smoke.StartupMap'
    )
    $unitState = @(
        'FogOfCommand.UnitState.Identifiers',
        'FogOfCommand.UnitState.Lifecycle',
        'FogOfCommand.UnitState.Resources',
        'FogOfCommand.UnitState.Validation',
        'FogOfCommand.UnitState.RepeatedOperations',
        'FogOfCommand.UnitState.Serialization'
    )
    switch ($Suite) {
        'Smoke' { $smoke }
        'UnitState' { $unitState }
        'All' { $smoke; $unitState }
    }
}

function Test-FocCounter($Value) {
    # JSON counters must be scalar integers. PowerShell otherwise coerces bools
    # and compares arrays elementwise, which could turn malformed reports green.
    return (($Value -is [int] -or $Value -is [long]) -and $Value -ge 0)
}

function Test-FocAutomationReport {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)] $Report,
        [ValidateSet('All', 'Smoke', 'UnitState')][string]$Suite = 'All'
    )

    $expected = @(Get-FocExpectedTests -Suite $Suite)
    $problems = [Collections.Generic.List[string]]::new()
    foreach ($name in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess', 'tests')) {
        if ($null -eq $Report.PSObject.Properties[$name]) { $problems.Add("Missing report field: $name") }
    }
    foreach ($name in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess')) {
        if ($null -ne $Report.PSObject.Properties[$name] -and !(Test-FocCounter $Report.$name)) {
            $problems.Add("Invalid report counter type/value: $name")
        }
    }
    if ($null -ne $Report.PSObject.Properties['tests'] -and $Report.tests -isnot [array]) {
        $problems.Add('The tests field must be a JSON array.')
    }
    if ($problems.Count -eq 0) {
        if ($Report.succeeded -ne $expected.Count) { $problems.Add("Expected exactly $($expected.Count) successful tests for suite $Suite.") }
        foreach ($name in @('succeededWithWarnings', 'failed', 'notRun', 'inProcess')) {
            if ($Report.$name -ne 0) { $problems.Add("Nonzero report counter: $name") }
        }
        $tests = @($Report.tests)
        $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        if ($tests.Count -ne $expected.Count) { $problems.Add('Unexpected test count; zero or partial runs cannot pass.') }
        foreach ($test in $tests) {
            $complete = $true
            foreach ($name in @('fullTestPath', 'state', 'warnings', 'errors')) {
                if ($null -eq $test -or $null -eq $test.PSObject.Properties[$name]) {
                    $problems.Add("Missing test field: $name")
                    $complete = $false
                }
            }
            if (!$complete) { continue }
            if ($test.fullTestPath -isnot [string] -or $test.state -isnot [string] -or
                !(Test-FocCounter $test.warnings) -or !(Test-FocCounter $test.errors)) {
                $problems.Add('Invalid test field types or counter values.')
                continue
            }
            if ($test.fullTestPath -cnotin $expected) { $problems.Add('Unexpected test name in exported report.') }
            if (!$seen.Add([string]$test.fullTestPath)) { $problems.Add('Duplicate test in exported report.') }
            if ($test.state -cne 'Success' -or $test.errors -ne 0 -or $test.warnings -ne 0) {
                $problems.Add('A test failed, was skipped/incomplete, or emitted warnings/errors.')
            }
        }
        foreach ($name in $expected) {
            if (!$seen.Contains($name)) { $problems.Add("Expected test missing: $name") }
        }
    }
    return [pscustomobject]@{
        Status = $(if ($problems.Count -eq 0) { 'PASS' } else { 'FAIL' })
        Problems = @($problems.ToArray())
    }
}

# Pure validation of the UE 5.8 Automation index.json contract. No UE launch or IO.
function Test-FocAutomationReport {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)] $Report)

    $expected = @(
        'FogOfCommand.Smoke.Configuration',
        'FogOfCommand.Smoke.Module',
        'FogOfCommand.Smoke.StartupMap'
    )
    $problems = [Collections.Generic.List[string]]::new()
    foreach ($name in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess', 'tests')) {
        if ($null -eq $Report.PSObject.Properties[$name]) { $problems.Add("Missing report field: $name") }
    }
    if ($problems.Count -eq 0) {
        if ($Report.succeeded -ne $expected.Count) { $problems.Add('Expected exactly three successful tests.') }
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

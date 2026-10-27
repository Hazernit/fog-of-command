# Pure registry-output parsing; no process launch or IO.
function Get-FocRegisteredEventDebugCommands {
    param([string[]]$Lines)
    foreach ($line in $Lines) {
        if ($line -cmatch 'Cmd:|commandline=|Command Line:') { continue }
        # FOutputDevice can emit bare names after the UE timestamp/frame prefix,
        # or after a log-category prefix. A command execution echo is not a name.
        $entry = [regex]::Match($line, '(?:^|\]\s*|:\s*)\s*(foc\.EventLog\.\w+)\s*$')
        if ($entry.Success) { $entry.Groups[1].Value }
    }
}

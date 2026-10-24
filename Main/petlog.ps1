# Runs a compile/run command, shows its output as usual, and tells the pet how it went
# by writing activity.log next to this script (the game reads it every 2 seconds).
#   powershell -ExecutionPolicy Bypass -File petlog.ps1 g++ main.cpp -o main.exe
$Command = $args[0]
$Rest = @($args | Select-Object -Skip 1)
$exe = (Get-Command $Command -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $exe) { Write-Host "'$Command' is not installed or not on PATH."; exit 1 }

$psi = New-Object System.Diagnostics.ProcessStartInfo $exe
$psi.Arguments = ($Rest | ForEach-Object { if ($_ -eq '' -or $_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join ' '
$psi.WorkingDirectory = (Get-Location).ProviderPath # PowerShell's cd doesn't change .NET's current folder
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true # only stderr is captured, so programs that ask for input still work
$p = [System.Diagnostics.Process]::Start($psi)
$errors = New-Object System.Collections.Generic.List[string]
while ($null -ne ($line = $p.StandardError.ReadLine())) {
    [Console]::Error.WriteLine($line)
    $errors.Add(($line -replace "\x1b\[[0-9;]*[A-Za-z]", "")) # drop colour codes
}
$p.WaitForExit()

$status = if ($p.ExitCode -eq 0) { "COMPILE_SUCCESS" } else { "COMPILE_ERROR" }
Set-Content -Path (Join-Path $PSScriptRoot "activity.log") -Value (@($status) + $errors) -Encoding ascii
exit $p.ExitCode

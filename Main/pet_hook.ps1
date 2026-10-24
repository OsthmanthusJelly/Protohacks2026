# Loaded by the "Tamagotchi" VS Code terminal (see .vscode/settings.json):
# compile and run commands typed there also report to the pet through petlog.ps1.
$petlog = Join-Path $PSScriptRoot "petlog.ps1"
foreach ($cmd in 'g++', 'gcc', 'c++', 'clang', 'clang++', 'python', 'python3', 'py', 'node', 'javac', 'java') {
    Set-Item "function:global:$cmd" ([scriptblock]::Create("& '$petlog' '$cmd' @args"))
}
Write-Host "Tamagothu is watching your compiles in this terminal." -ForegroundColor Green

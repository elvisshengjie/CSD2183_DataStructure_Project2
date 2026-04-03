$ErrorActionPreference = "Stop"

$repoWin = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoWsl = "/mnt/" + $repoWin.Substring(0, 1).ToLower() + $repoWin.Substring(2).Replace("\", "/")

wsl bash -lc "cd $repoWsl && make" | Out-Null
wsl bash -lc "cd $repoWsl && ./simplify tests/data/example_input.csv 12" | Out-File -Encoding ascii tests/smoke_output.tmp
Compare-Object `
    (Get-Content tests/data/example_expected_noop.txt) `
    (Get-Content tests/smoke_output.tmp) `
    -SyncWindow 0 | ForEach-Object {
        throw "Smoke test failed."
    }
Remove-Item tests/smoke_output.tmp -ErrorAction SilentlyContinue
Write-Host "Smoke test passed."

$ErrorActionPreference = "Stop"

make
./simplify tests/data/example_input.csv 12 | Out-File -Encoding ascii tests/smoke_output.tmp
Compare-Object `
    (Get-Content tests/data/example_expected_noop.txt) `
    (Get-Content tests/smoke_output.tmp) `
    -SyncWindow 0 | ForEach-Object {
        throw "Smoke test failed."
    }
Remove-Item tests/smoke_output.tmp -ErrorAction SilentlyContinue
Write-Host "Smoke test passed."

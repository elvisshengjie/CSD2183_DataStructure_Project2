$ErrorActionPreference = "Stop"

$repoWin = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoWsl = "/mnt/" + $repoWin.Substring(0, 1).ToLower() + $repoWin.Substring(2).Replace("\", "/")

wsl bash -lc "cd $repoWsl && python3 tests/custom/generate_custom_datasets.py"
wsl bash -lc "cd $repoWsl && make"
wsl bash -lc "cd $repoWsl && python3 benchmarks/run_benchmarks.py"

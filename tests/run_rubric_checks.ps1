$ErrorActionPreference = "Stop"

$repoWin = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoWsl = "/mnt/" + $repoWin.Substring(0, 1).ToLower() + $repoWin.Substring(2).Replace("\", "/")
$tmpDir = Join-Path $PSScriptRoot ".rubric_tmp"

$cases = @(
    @{ Input = "input_rectangle_with_two_holes.csv"; Target = 7 },
    @{ Input = "input_cushion_with_hexagonal_hole.csv"; Target = 13 },
    @{ Input = "input_blob_with_two_holes.csv"; Target = 17 },
    @{ Input = "input_wavy_with_three_holes.csv"; Target = 21 },
    @{ Input = "input_lake_with_two_islands.csv"; Target = 17 },
    @{ Input = "input_original_01.csv"; Target = 99 },
    @{ Input = "input_original_02.csv"; Target = 99 },
    @{ Input = "input_original_03.csv"; Target = 99 },
    @{ Input = "input_original_04.csv"; Target = 99 },
    @{ Input = "input_original_05.csv"; Target = 99 },
    @{ Input = "input_original_06.csv"; Target = 99 },
    @{ Input = "input_original_07.csv"; Target = 99 },
    @{ Input = "input_original_08.csv"; Target = 99 },
    @{ Input = "input_original_09.csv"; Target = 99 },
    @{ Input = "input_original_10.csv"; Target = 99 }
)

New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
wsl bash -lc "cd $repoWsl && make" | Out-Null

$failures = 0

foreach ($case in $cases) {
    $outputName = [System.IO.Path]::GetFileNameWithoutExtension($case.Input) + ".out.txt"
    $outputPath = Join-Path $tmpDir $outputName
    wsl bash -lc "cd $repoWsl && timeout 30s ./simplify tests/$($case.Input) $($case.Target)" | Out-File -Encoding ascii $outputPath

    if ($LASTEXITCODE -eq 124) {
        Write-Host "$($case.Input) `t FAIL (timeout)"
        $failures++
        continue
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Host "$($case.Input) `t FAIL (program error $LASTEXITCODE)"
        $failures++
        continue
    }

    python (Join-Path $PSScriptRoot "validate_output.py") `
        --input (Join-Path $PSScriptRoot $case.Input) `
        --output $outputPath `
        --target $case.Target

    if ($LASTEXITCODE -eq 0) {
        Write-Host "$($case.Input) `t PASS"
    } else {
        Write-Host "$($case.Input) `t FAIL"
        $failures++
    }
}

if ($failures -gt 0) {
    throw "$failures rubric check(s) failed."
}

Write-Host "All rubric checks passed."

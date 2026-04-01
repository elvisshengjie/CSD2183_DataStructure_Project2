$ErrorActionPreference = "Stop"

$repoWin = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoWsl = "/mnt/" + $repoWin.Substring(0, 1).ToLower() + $repoWin.Substring(2).Replace("\", "/")

$cases = @(
    @{ Input = "input_rectangle_with_two_holes.csv"; Target = 7; Expected = "output_rectangle_with_two_holes.txt" },
    @{ Input = "input_cushion_with_hexagonal_hole.csv"; Target = 13; Expected = "output_cushion_with_hexagonal_hole.txt" },
    @{ Input = "input_blob_with_two_holes.csv"; Target = 17; Expected = "output_blob_with_two_holes.txt" },
    @{ Input = "input_wavy_with_three_holes.csv"; Target = 21; Expected = "output_wavy_with_three_holes.txt" },
    @{ Input = "input_lake_with_two_islands.csv"; Target = 17; Expected = "output_lake_with_two_islands.txt" },
    @{ Input = "input_original_01.csv"; Target = 99; Expected = "output_original_01.txt" },
    @{ Input = "input_original_02.csv"; Target = 99; Expected = "output_original_02.txt" },
    @{ Input = "input_original_03.csv"; Target = 99; Expected = "output_original_03.txt" },
    @{ Input = "input_original_04.csv"; Target = 99; Expected = "output_original_04.txt" },
    @{ Input = "input_original_05.csv"; Target = 99; Expected = "output_original_05.txt" },
    @{ Input = "input_original_06.csv"; Target = 99; Expected = "output_original_06.txt" },
    @{ Input = "input_original_07.csv"; Target = 99; Expected = "output_original_07.txt" },
    @{ Input = "input_original_08.csv"; Target = 99; Expected = "output_original_08.txt" },
    @{ Input = "input_original_09.csv"; Target = 99; Expected = "output_original_09.txt" },
    @{ Input = "input_original_10.csv"; Target = 99; Expected = "output_original_10.txt" }
)

wsl bash -lc "cd $repoWsl && make" | Out-Null

$failures = 0

foreach ($case in $cases) {
    $actual = wsl bash -lc "cd $repoWsl && timeout 15s ./simplify tests/$($case.Input) $($case.Target)"
    $exitCode = $LASTEXITCODE

    if ($exitCode -eq 124) {
        Write-Host "$($case.Input) `t TIMEOUT"
        $failures++
        continue
    }

    if ($exitCode -ne 0) {
        Write-Host "$($case.Input) `t ERROR($exitCode)"
        $failures++
        continue
    }

    $expected = Get-Content (Join-Path $PSScriptRoot $case.Expected)
    $diff = Compare-Object $expected $actual -SyncWindow 0

    if ($null -eq $diff) {
        Write-Host "$($case.Input) `t PASS"
    } else {
        Write-Host "$($case.Input) `t FAIL"
        $failures++
    }
}

if ($failures -gt 0) {
    throw "$failures fixture test(s) did not pass."
}

Write-Host "All fixture tests passed."

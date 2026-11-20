# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Yukinoshita-lin
param(
  [string]$ExePath = ""
)
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExePath)) {
  $ExePath = Join-Path $ProjectRoot "DNAExpressionSim.exe"
}
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path
$Work = Join-Path $ProjectRoot ("build\cli-tests-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $Work | Out-Null
$script:Passed = 0

function Invoke-DnaApp {
  param([string[]]$Arguments)
  $quoted = $Arguments | ForEach-Object {
    if ($_ -match '\s') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
  }
  $process = Start-Process -FilePath $ExePath -ArgumentList ($quoted -join " ") -WindowStyle Hidden -Wait -PassThru
  $exitCode = $process.ExitCode
  $process.Dispose()
  return $exitCode
}

function Assert-DnaTest {
  param([bool]$Condition, [string]$Name)
  if (-not $Condition) { throw "FAIL $Name" }
  $script:Passed++
  Write-Output "  ok   $Name"
}

try {
  $dna = Join-Path $Work "dna.txt"
  $protein = Join-Path $Work "protein.txt"
  $invalid = Join-Path $Work "invalid.txt"
  $missing = Join-Path $Work "missing\out.txt"

  $result = Invoke-DnaApp @("--sample=1", "--export-dna=$dna", "--export-protein=$protein", "--exit")
  Assert-DnaTest ($result -eq 0 -and (Test-Path -LiteralPath $dna) -and (Test-Path -LiteralPath $protein)) "valid DNA and protein exports return success"
  Assert-DnaTest ([IO.File]::ReadAllText($dna).Contains("length=76 bp")) "DNA export contains sample 1"
  $proteinBytes = [IO.File]::ReadAllBytes($protein)
  Assert-DnaTest ($proteinBytes.Length -gt 3 -and $proteinBytes[0] -eq 0xEF -and $proteinBytes[1] -eq 0xBB -and $proteinBytes[2] -eq 0xBF) "protein export contains UTF-8 BOM"

  $result = Invoke-DnaApp @("--sample=1", "--export-dna=$missing", "--exit")
  Assert-DnaTest ($result -eq 1 -and -not (Test-Path -LiteralPath $missing)) "invalid export path returns exit code 1"

  $result = Invoke-DnaApp @("--seq=ATGbxx", "--export-dna=$invalid", "--exit")
  Assert-DnaTest ($result -eq 1 -and -not (Test-Path -LiteralPath $invalid)) "invalid lowercase sequence is not exported"

  $before = [IO.File]::ReadAllText($dna)
  $result = Invoke-DnaApp @("--sample=2", "--export-dna=$dna", "--exit")
  Assert-DnaTest ($result -eq 1 -and [IO.File]::ReadAllText($dna) -eq $before) "existing file is preserved without force"

  $result = Invoke-DnaApp @("--sample=2", "--export-dna=$dna", "--force", "--exit")
  Assert-DnaTest ($result -eq 0 -and [IO.File]::ReadAllText($dna).Contains("length=48 bp")) "force explicitly overwrites existing file"

  $result = Invoke-DnaApp @("--sample=9", "--exit")
  Assert-DnaTest ($result -eq 2) "invalid sample returns exit code 2"
  $result = Invoke-DnaApp @("--progress=1.5", "--exit")
  Assert-DnaTest ($result -eq 2) "out-of-range progress returns exit code 2"
  $result = Invoke-DnaApp @("--frame=abc", "--exit")
  Assert-DnaTest ($result -eq 2) "non-numeric frame returns exit code 2"
  $result = Invoke-DnaApp @("--unknown-option", "--exit")
  Assert-DnaTest ($result -eq 2) "unknown option returns exit code 2"
  $result = Invoke-DnaApp @("--export-dna=", "--exit")
  Assert-DnaTest ($result -eq 2) "empty export path returns exit code 2"
  Assert-DnaTest (@(Get-ChildItem -LiteralPath $Work -Filter "dna*.tmp").Count -eq 0) "no temporary files remain"

  Write-Output "`n$script:Passed passed, 0 failed"
} finally {
  if (Test-Path -LiteralPath $Work) { Remove-Item -LiteralPath $Work -Recurse -Force }
}

param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot "gateway.windows.json")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
    throw "Gateway config not found: $ConfigPath"
}

$config = Get-Content -LiteralPath $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
$gatewayRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$gatewayScript = Join-Path $gatewayRoot "gateway.py"

if (-not (Test-Path -LiteralPath $gatewayScript -PathType Leaf)) {
    throw "gateway.py not found: $gatewayScript"
}

$pythonExe = [string]$config.pythonExe
if ([string]::IsNullOrWhiteSpace($pythonExe)) {
    $pythonCommand = Get-Command python3.exe, python.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $pythonCommand) {
        throw "Python 3 was not found. Set pythonExe in gateway.windows.json."
    }
    $pythonExe = $pythonCommand.Source
}

$gatewayArgs = @(
    "-u",
    "-B",
    $gatewayScript,
    "--host", [string]$config.host,
    "--port", [string]$config.port,
    "--database", [string]$config.database,
    "--public-base-url", [string]$config.publicBaseUrl,
    "--upstream-timeout", [string]$config.upstreamTimeout,
    "--max-body-mb", [string]$config.maxBodyMb,
    "--redeem-attempts-per-minute", [string]$config.redeemAttemptsPerMinute,
    "--max-workers", [string]$config.maxWorkers
)

$logPath = Join-Path $gatewayRoot "data\gateway.log"

while ($true) {
    Add-Content -LiteralPath $logPath -Value "$(Get-Date -Format o) starting Mosaic gateway"
    & $pythonExe @gatewayArgs 2>&1 | ForEach-Object {
        $_ | Out-File -LiteralPath $logPath -Append -Encoding UTF8
    }
    $gatewayExitCode = $LASTEXITCODE
    Add-Content -LiteralPath $logPath -Value "$(Get-Date -Format o) gateway exited: $gatewayExitCode; restarting in 3s"
    Start-Sleep -Seconds 3
}

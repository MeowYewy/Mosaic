param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot "gateway.windows.json"),
    [int]$Port = 8788,
    [string]$BasePath = ""
)

$ErrorActionPreference = "Stop"

$gatewayRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$adminScript = Join-Path $gatewayRoot "admin_web.py"
if (-not (Test-Path -LiteralPath $adminScript -PathType Leaf)) {
    throw "admin_web.py not found: $adminScript"
}

$pythonExe = ""
$databasePath = Join-Path $gatewayRoot "data\gateway.sqlite3"
$credentialsPath = Join-Path $gatewayRoot "data\admin_credentials.json"
if (Test-Path -LiteralPath $ConfigPath -PathType Leaf) {
    $config = Get-Content -LiteralPath $ConfigPath -Raw -Encoding UTF8 |
        ConvertFrom-Json
    $pythonExe = [string]$config.pythonExe
    if (-not [string]::IsNullOrWhiteSpace([string]$config.database)) {
        $databasePath = [string]$config.database
    }
}

if ([string]::IsNullOrWhiteSpace($pythonExe)) {
    $pythonCommand = Get-Command python3.exe, python.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $pythonCommand) {
        throw "Python 3 was not found."
    }
    $pythonExe = $pythonCommand.Source
}

$args = @(
    "-B", $adminScript,
    "--database", $databasePath,
    "--credentials", $credentialsPath,
    "--host", "127.0.0.1",
    "--port", $Port,
    "--open-browser"
)
if (-not [string]::IsNullOrWhiteSpace($BasePath)) {
    $args += @("--base-path", $BasePath)
}

& $pythonExe @args
exit $LASTEXITCODE

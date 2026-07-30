param(
    [Parameter(Mandatory = $true)]
    [string]$PublicBaseUrl,

    [string]$ListenHost = "127.0.0.1",
    [int]$Port = 8787,
    [string]$PythonExe = "",
    [string]$TaskName = "Mosaic Redemption Gateway",
    [switch]$OpenFirewall
)

$ErrorActionPreference = "Stop"

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run PowerShell as Administrator, then execute this script again."
}

$gatewayRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$gatewayScript = Join-Path $gatewayRoot "gateway.py"
$adminScript = Join-Path $gatewayRoot "admin.py"
$runScript = Join-Path $PSScriptRoot "run-gateway.ps1"

foreach ($requiredFile in @($gatewayScript, $adminScript, $runScript)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required file is missing: $requiredFile"
    }
}

if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonCommand = Get-Command python3.exe, python.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $pythonCommand) {
        throw "Python 3 was not found. Install it or pass the real python.exe path with -PythonExe."
    }
    $PythonExe = $pythonCommand.Source
}
$PythonExe = (Resolve-Path -LiteralPath $PythonExe).Path
if ($PythonExe -like "*\WindowsApps\python*.exe") {
    throw "The Microsoft Store Python alias cannot reliably run as SYSTEM. Install Python system-wide and pass the real python.exe path with -PythonExe."
}
& $PythonExe --version | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Python 3 cannot run: $PythonExe"
}

$parsedPublicUrl = $null
if (-not [Uri]::TryCreate($PublicBaseUrl, [UriKind]::Absolute, [ref]$parsedPublicUrl) -or
    $parsedPublicUrl.Scheme -notin @("http", "https")) {
    throw "PublicBaseUrl must be an absolute http:// or https:// URL."
}

$dataDir = Join-Path $gatewayRoot "data"
New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
$databasePath = Join-Path $dataDir "gateway.sqlite3"
$configPath = Join-Path $PSScriptRoot "gateway.windows.json"

$config = [ordered]@{
    pythonExe = $PythonExe
    host = $ListenHost
    port = $Port
    publicBaseUrl = $PublicBaseUrl.TrimEnd("/")
    database = $databasePath
    upstreamTimeout = 1200
    maxBodyMb = 20
    redeemAttemptsPerMinute = 20
    maxWorkers = 8
}
$config | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8

& $PythonExe -B $adminScript --database $databasePath init
if ($LASTEXITCODE -ne 0) {
    throw "Database initialization failed."
}

$taskArgument = "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$runScript`""
$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument $taskArgument
$trigger = New-ScheduledTaskTrigger -AtStartup
$taskSettings = New-ScheduledTaskSettingsSet `
    -RestartCount 10 `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -MultipleInstances IgnoreNew `
    -StartWhenAvailable
$taskPrincipal = New-ScheduledTaskPrincipal `
    -UserId "SYSTEM" `
    -LogonType ServiceAccount `
    -RunLevel Highest

Register-ScheduledTask `
    -TaskName $TaskName `
    -Action $action `
    -Trigger $trigger `
    -Settings $taskSettings `
    -Principal $taskPrincipal `
    -Description "Mosaic redemption and AI proxy gateway" `
    -Force | Out-Null

if ($OpenFirewall) {
    $firewallName = "Mosaic Redemption Gateway TCP $Port"
    if (-not (Get-NetFirewallRule -DisplayName $firewallName -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule `
            -DisplayName $firewallName `
            -Direction Inbound `
            -Action Allow `
            -Protocol TCP `
            -LocalPort $Port | Out-Null
    }
}

Start-ScheduledTask -TaskName $TaskName
Start-Sleep -Seconds 2

$healthUrl = "http://127.0.0.1:$Port/healthz"
try {
    $health = Invoke-RestMethod -Uri $healthUrl -TimeoutSec 5
    if (-not $health.ok) {
        throw "The health check returned an unexpected response."
    }
    Write-Host "Installation complete. Gateway is running: $healthUrl" -ForegroundColor Green
    Write-Host "Scheduled task: $TaskName"
    Write-Host "Public client/reverse-proxy URL: $($config.publicBaseUrl)"
} catch {
    Write-Warning "The scheduled task was created, but the health check failed: $($_.Exception.Message)"
    Write-Warning "Inspect the '$TaskName' task in Task Scheduler."
}

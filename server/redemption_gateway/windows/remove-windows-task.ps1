param(
    [string]$TaskName = "Mosaic Redemption Gateway",
    [int]$Port = 8787
)

$ErrorActionPreference = "Stop"

if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Stop-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    Write-Host "Removed scheduled task: $TaskName"
}

$firewallName = "Mosaic Redemption Gateway TCP $Port"
Get-NetFirewallRule -DisplayName $firewallName -ErrorAction SilentlyContinue |
    Remove-NetFirewallRule

Write-Host "The database and redemption codes were not deleted."

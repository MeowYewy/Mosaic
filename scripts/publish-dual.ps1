# Publish Mosaic v{APP_VERSION} to GitHub + Gitee Releases (installer + portable).
# Requires: GITHUB_TOKEN or GH_TOKEN, and GITEE_TOKEN in the environment.
param(
    [string]$Version = "",
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

function Get-VersionFromFile {
    param([string]$Root)
    $path = Join-Path $Root "APP_VERSION.txt"
    if (-not (Test-Path $path)) { throw "APP_VERSION.txt not found" }
    return (Get-Content $path -Raw).Trim()
}

function Get-ReleaseNotes {
    param([string]$Root)
    $path = Join-Path $Root "resources\changelog.json"
    if (-not (Test-Path $path)) { return "Mosaic release" }
    $entries = Get-Content $path -Raw | ConvertFrom-Json
    foreach ($entry in $entries) {
        if ($entry.version -eq $Version) {
            return ($entry.notes.zh_CN -replace "`n", "`n")
        }
    }
    return "Mosaic v$Version"
}

function Invoke-GitHubRelease {
    param(
        [string]$Token,
        [string]$Owner,
        [string]$Repo,
        [string]$Tag,
        [string]$Title,
        [string]$Body,
        [string[]]$Assets
    )

    $headers = @{
        Authorization = "Bearer $Token"
        Accept        = "application/vnd.github+json"
        "X-GitHub-Api-Version" = "2022-11-28"
    }

    $existing = $null
    try {
        $existing = Invoke-RestMethod -Uri "https://api.github.com/repos/$Owner/$Repo/releases/tags/$Tag" -Headers $headers
    } catch { }

    if ($existing -and $existing.id) {
        Write-Host "GitHub release $Tag already exists (id=$($existing.id)), reusing."
        $release = $existing
    } else {
        $payload = @{
            tag_name         = $Tag
            name             = $Title
            body             = $Body
            draft            = $false
            prerelease       = $false
            generate_release_notes = $false
            target_commitish = "main"
        } | ConvertTo-Json
        $release = Invoke-RestMethod -Method Post -Uri "https://api.github.com/repos/$Owner/$Repo/releases" -Headers $headers -Body $payload -ContentType "application/json; charset=utf-8"
        Write-Host "Created GitHub release $Tag (id=$($release.id))"
    }

    foreach ($assetPath in $Assets) {
        $name = [IO.Path]::GetFileName($assetPath)
        $existingAsset = $release.assets | Where-Object { $_.name -eq $name }
        if ($existingAsset) {
            Write-Host "Deleting existing GitHub asset: $name"
            Invoke-RestMethod -Method Delete -Uri "https://api.github.com/repos/$Owner/$Repo/releases/assets/$($existingAsset.id)" -Headers $headers | Out-Null
        }
        Write-Host "Uploading to GitHub: $name ..."
        $uploadHeaders = @{
            Authorization = "Bearer $Token"
            Accept        = "application/vnd.github+json"
            "Content-Type" = "application/octet-stream"
        }
        $bytes = [IO.File]::ReadAllBytes($assetPath)
        Invoke-RestMethod -Method Post -Uri "https://uploads.github.com/repos/$Owner/$Repo/releases/$($release.id)/assets?name=$name" -Headers $uploadHeaders -Body $bytes | Out-Null
        Write-Host "  OK"
    }

    return $release.html_url
}

function Invoke-GiteeRelease {
    param(
        [string]$Token,
        [string]$Owner,
        [string]$Repo,
        [string]$Tag,
        [string]$Title,
        [string]$Body,
        [string[]]$Assets
    )

    $base = "https://gitee.com/api/v5/repos/$Owner/$Repo"
    $existing = $null
    try {
        $existing = Invoke-RestMethod -Uri "$base/releases/tags/$Tag?access_token=$Token"
    } catch { }

    if ($existing -and $existing.id) {
        Write-Host "Gitee release $Tag already exists (id=$($existing.id)), reusing."
        $releaseId = $existing.id
    } else {
        $payload = @{
            access_token     = $Token
            tag_name         = $Tag
            name             = $Title
            body             = $Body
            target_commitish = "main"
            prerelease       = $false
        }
        $created = Invoke-RestMethod -Method Post -Uri "$base/releases" -Body $payload
        $releaseId = $created.id
        Write-Host "Created Gitee release $Tag (id=$releaseId)"
    }

    foreach ($assetPath in $Assets) {
        $name = [IO.Path]::GetFileName($assetPath)
        Write-Host "Uploading to Gitee: $name ..."
        $form = @{
            access_token = $Token
            file         = Get-Item -LiteralPath $assetPath
        }
        Invoke-RestMethod -Method Post -Uri "$base/releases/$releaseId/attach_files" -Form $form | Out-Null
        Write-Host "  OK"
    }

    return "https://gitee.com/$Owner/$Repo/releases/tag/$Tag"
}

if (-not $Version) { $Version = Get-VersionFromFile -Root $ProjectRoot }
$tag = "v$Version"
$artifactDir = Join-Path $ProjectRoot "dist\artifacts"
$portable = Join-Path $artifactDir "Mosaic_${Version}_win64_portable.zip"
$setup = Join-Path $artifactDir "Mosaic_${Version}_win64_Setup.exe"

foreach ($path in @($portable, $setup)) {
    if (-not (Test-Path $path)) {
        throw "Missing artifact: $path`nRun release.bat first."
    }
}

$ghToken = $env:GITHUB_TOKEN
if (-not $ghToken) { $ghToken = $env:GH_TOKEN }
$giteeToken = $env:GITEE_TOKEN

if (-not $ghToken) { throw "Set GITHUB_TOKEN or GH_TOKEN for GitHub release upload." }
if (-not $giteeToken) { throw "Set GITEE_TOKEN for Gitee release upload." }

$notes = Get-ReleaseNotes -Root $ProjectRoot
$title = "Mosaic v$Version"
$assets = @($portable, $setup)

Write-Host ""
Write-Host "=== Publishing $title to GitHub + Gitee ==="
Write-Host ""

$ghUrl = Invoke-GitHubRelease -Token $ghToken -Owner "MeowYewy" -Repo "Mosaic" -Tag $tag -Title $title -Body $notes -Assets $assets
$giteeUrl = Invoke-GiteeRelease -Token $giteeToken -Owner "MeowYewy" -Repo "mosaic" -Tag $tag -Title $title -Body $notes -Assets $assets

Write-Host ""
Write-Host "GitHub: $ghUrl"
Write-Host "Gitee:  $giteeUrl"
Write-Host ""
Write-Host "Ensure resources/update.json on main matches (GitHub or Gitee — app picks the faster source):"
Write-Host "  https://github.com/MeowYewy/Mosaic/releases/download/$tag/Mosaic_${Version}_win64_Setup.exe"
Write-Host "  https://gitee.com/MeowYewy/mosaic/releases/download/$tag/Mosaic_${Version}_win64_Setup.exe"

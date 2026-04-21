[CmdletBinding()]
param(
    [string]$Repo = "thomashamer3/MiFutbolC-GUI",
    [string]$Version = "v1.0.0",
    [string]$AssetName = "MiFutbolC_Setup.exe",
    [switch]$Silent,
    [switch]$KeepFile,
    [switch]$ForceUpdate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Log {
    param([string]$Message)
    Write-Host "[MiFutbolC] $Message"
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[MiFutbolC][WARN] $Message" -ForegroundColor Yellow
}

function Test-EnvTrue {
    param([string]$Name)

    $rawValue = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($rawValue)) {
        return $false
    }

    switch ($rawValue.Trim().ToLowerInvariant()) {
        "1" { return $true }
        "true" { return $true }
        "yes" { return $true }
        "y" { return $true }
        "on" { return $true }
        default { return $false }
    }
}

if (-not ($env:OS -eq "Windows_NT")) {
    throw "Este instalador es solo para Windows. En Linux/macOS usa install.sh."
}

$gitCommand = Get-Command git -ErrorAction SilentlyContinue
if ($null -eq $gitCommand) {
    Write-Warn "git no esta instalado. No bloquea instalacion por binario."
}
else {
    Write-Log "git detectado."
}

$makeCommand = Get-Command make -ErrorAction SilentlyContinue
if ($null -eq $makeCommand) {
    Write-Warn "make no esta instalado. No bloquea instalacion por binario."
}
else {
    Write-Log "make detectado."
}

$repoFromEnv = [Environment]::GetEnvironmentVariable("MIFUTBOLC_REPO")
if (-not [string]::IsNullOrWhiteSpace($repoFromEnv)) {
    $Repo = $repoFromEnv
}

$versionFromEnv = [Environment]::GetEnvironmentVariable("MIFUTBOLC_VERSION")
if (-not [string]::IsNullOrWhiteSpace($versionFromEnv)) {
    $Version = $versionFromEnv
}

$assetFromEnv = [Environment]::GetEnvironmentVariable("MIFUTBOLC_ASSET_NAME")
if (-not [string]::IsNullOrWhiteSpace($assetFromEnv)) {
    $AssetName = $assetFromEnv
}

if (Test-EnvTrue -Name "MIFUTBOLC_SILENT") {
    $Silent = $true
}

if (Test-EnvTrue -Name "MIFUTBOLC_KEEP_FILE") {
    $KeepFile = $true
}

if (Test-EnvTrue -Name "MIFUTBOLC_FORCE_UPDATE") {
    $ForceUpdate = $true
}

$versionStateDir = Join-Path -Path $env:LOCALAPPDATA -ChildPath "MiFutbolC"
$versionStateFile = Join-Path -Path $versionStateDir -ChildPath "installer-version.txt"

if ((-not $ForceUpdate) -and (Test-Path -LiteralPath $versionStateFile)) {
    $installedVersion = (Get-Content -LiteralPath $versionStateFile -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($installedVersion -eq $Version) {
        Write-Log "La version $Version ya esta instalada. No se realizan cambios."
        Write-Log "Para reinstalar, define MIFUTBOLC_FORCE_UPDATE=1."
        return
    }
}

$downloadUrl = "https://github.com/$Repo/releases/download/$Version/$AssetName"
$tempRoot = Join-Path -Path ([IO.Path]::GetTempPath()) -ChildPath ("mifutbolc-install-" + [Guid]::NewGuid().ToString("N"))
$installerPath = Join-Path -Path $tempRoot -ChildPath $AssetName

New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
    Write-Log "Descargando instalador versionado: $downloadUrl"

    $requestParams = @{
        Uri     = $downloadUrl
        OutFile = $installerPath
    }
    if ($PSVersionTable.PSVersion.Major -lt 6) {
        $requestParams["UseBasicParsing"] = $true
    }

    Invoke-WebRequest @requestParams

    if (-not (Test-Path -LiteralPath $installerPath)) {
        throw "No se pudo descargar el instalador release."
    }

    $signature = Get-Content -LiteralPath $installerPath -Encoding Byte -TotalCount 2
    if ($signature.Length -lt 2 -or $signature[0] -ne 77 -or $signature[1] -ne 90) {
        throw "El archivo descargado no parece un ejecutable valido (.exe)."
    }

    $startArgs = @()
    if ($Silent) {
        $startArgs += "/VERYSILENT"
        $startArgs += "/SUPPRESSMSGBOXES"
        $startArgs += "/NORESTART"
    }

    Write-Log "Ejecutando instalador..."
    $process = Start-Process -FilePath $installerPath -ArgumentList $startArgs -Wait -PassThru
    if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) {
        throw "El instalador termino con codigo $($process.ExitCode)."
    }

    New-Item -ItemType Directory -Path $versionStateDir -Force | Out-Null
    Set-Content -LiteralPath $versionStateFile -Value $Version -Encoding ASCII

    Write-Log "Instalacion completada."
    Write-Log "Version instalada: $Version"
}
finally {
    if (-not $KeepFile -and (Test-Path -LiteralPath $tempRoot)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

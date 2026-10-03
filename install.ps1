# Instaluje wtyczke w Photoshop Elements / Photoshop (wymaga uprawnien administratora - skrypt sam o nie poprosi).
# Uzycie: powershell -ExecutionPolicy Bypass -File install.ps1 [-Plugin sciezka\do\ZetmarWatermark_64.8bf]
param([string]$Plugin = "$PSScriptRoot\build\ZetmarWatermark_64.8bf", [switch]$Uninstall)

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$PSCommandPath`"",'-Plugin',"`"$Plugin`"")
    if ($Uninstall) { $argList += '-Uninstall' }
    Start-Process powershell -Verb RunAs -ArgumentList $argList -Wait
    exit
}

$targets = @()
foreach ($root in 'C:\Program Files\Adobe') {
    if (-not (Test-Path $root)) { continue }
    Get-ChildItem $root -Directory | Where-Object { $_.Name -match '^Adobe Photoshop \d|^Adobe Photoshop$|^Photoshop Elements \d{4}$' } | ForEach-Object {
        $pl = Join-Path $_.FullName 'Plug-Ins'
        if (-not (Test-Path $pl)) { $pl = Join-Path $_.FullName 'Plug-ins' }
        if (Test-Path $pl) { $targets += $pl }
    }
}
if (-not $targets) { Write-Host 'Nie znaleziono instalacji Photoshopa / Elements.'; Read-Host 'Enter'; exit 1 }
foreach ($t in $targets) {
    $dir = Join-Path $t 'Zetmar'
    if ($Uninstall) { Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue; Write-Host "Usunieto: $dir"; continue }
    New-Item -ItemType Directory $dir -Force | Out-Null
    Copy-Item $Plugin $dir -Force
    Write-Host "Zainstalowano: $dir"
}
Start-Sleep 2

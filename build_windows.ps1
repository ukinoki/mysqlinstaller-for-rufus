# =============================================================================
#  MySQL Installer — Build Windows (qmake + nmake/jom/mingw, puis windeployqt)
# =============================================================================
#
#  PRÉREQUIS
#    • Qt 6.x pour Windows (kit MSVC de préférence — voir BUILD_WINDOWS.md).
#    • Le compilateur correspondant :
#        - MSVC : lancez ce script depuis « x64 Native Tools Command Prompt
#                 for VS 2022 » (cl.exe + nmake sur le PATH), puis :
#                     powershell -ExecutionPolicy Bypass -File build_windows.ps1
#        - MinGW : la version mingw de Qt fournit mingw32-make + g++.
#
#  CONFIG (optionnel) — si qmake n'est pas sur le PATH, indiquez le bin de Qt :
#        $env:QT_BIN = "C:\Qt\6.10.2\msvc2022_64\bin"
#        powershell -ExecutionPolicy Bypass -File build_windows.ps1
# =============================================================================

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

function Resolve-Tool([string]$name) {
    if ($env:QT_BIN) {
        $p = Join-Path $env:QT_BIN ($name + ".exe")
        if (Test-Path $p) { return $p }
    }
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

Write-Host "== MySQL Installer — build Windows ==" -ForegroundColor Cyan

# --- 1. qmake -----------------------------------------------------------------
$qmake = Resolve-Tool "qmake"
if (-not $qmake) {
    throw "qmake introuvable. Ajoutez le dossier 'bin' de Qt au PATH, ou definissez `$env:QT_BIN."
}
Write-Host "qmake : $qmake"

# --- 2. Outil de build (nmake / jom / mingw32-make) ---------------------------
$make = $null
foreach ($candidate in @("jom", "nmake", "mingw32-make")) {
    $c = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($c) { $make = $c.Source; break }
}
if (-not $make) {
    throw "Aucun outil de build (nmake/jom/mingw32-make). Ouvrez une invite " +
          "'x64 Native Tools Command Prompt for VS' (MSVC) ou ajoutez MinGW au PATH."
}
Write-Host "make  : $make"

# --- 3. Configuration et compilation (out-of-source) --------------------------
$build = Join-Path $root "build-windows"
New-Item -ItemType Directory -Force -Path $build | Out-Null
Set-Location $build

Write-Host "`n-> qmake (release)..." -ForegroundColor Yellow
& $qmake (Join-Path $root "MySQLInstaller.pro") "CONFIG+=release"
if ($LASTEXITCODE -ne 0) { throw "qmake a echoue (code $LASTEXITCODE)." }

Write-Host "`n-> compilation..." -ForegroundColor Yellow
& $make
if ($LASTEXITCODE -ne 0) { throw "$make a echoue (code $LASTEXITCODE)." }

# --- 4. Localiser l'exe -------------------------------------------------------
$exe = Get-ChildItem -Path $build -Recurse -Filter "MySQLInstaller.exe" |
       Select-Object -First 1
if (-not $exe) { throw "MySQLInstaller.exe introuvable apres le build." }
Write-Host "`nBuild OK : $($exe.FullName)" -ForegroundColor Green

# --- 5. Déploiement du runtime Qt (windeployqt) -------------------------------
$windeployqt = Resolve-Tool "windeployqt"
if ($windeployqt) {
    Write-Host "`n-> windeployqt..." -ForegroundColor Yellow
    # --no-translations : les traductions sont deja embarquees dans le binaire
    # (CONFIG += embed_translations -> qrc :/i18n).
    & $windeployqt --release --no-translations --compiler-runtime $exe.FullName
    Write-Host "`nDossier autonome pret : $($exe.DirectoryName)" -ForegroundColor Green
} else {
    Write-Warning "windeployqt introuvable : l'exe ne sera pas autonome (DLL Qt manquantes)."
    Write-Warning "Lancez-le depuis une invite ou Qt est sur le PATH, ou definissez `$env:QT_BIN."
}

Write-Host "`nLancez l'application en tant qu'administrateur :" -ForegroundColor Cyan
Write-Host "  $($exe.FullName)"
Write-Host "(L'invite UAC s'affiche automatiquement grace au manifeste embarque.)"

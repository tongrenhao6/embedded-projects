$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

python -m PyInstaller --noconfirm --clean --onefile --console `
  --name BeehiveMonitor `
  --add-data "beehive_gateway\web;beehive_gateway\web" `
  --exclude-module PyQt5 `
  --exclude-module PySide6 `
  --hidden-import beehive_gateway.api `
  --hidden-import mysql.connector.locales.eng.client_error `
  beehive_gateway\launcher.py

if ($LASTEXITCODE -ne 0) {
  throw "PyInstaller build failed."
}

Copy-Item .env.example .\dist\BeehiveMonitor.env.example -Force
Write-Host "Created .\dist\BeehiveMonitor.exe"
Write-Host "Copy BeehiveMonitor.env.example to .env and set BEEHIVE_DB_PASSWORD before first launch."

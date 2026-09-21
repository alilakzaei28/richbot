$ErrorActionPreference = "Stop"

Write-Host "Configuring directory layout..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path "src" | Out-Null
New-Item -ItemType Directory -Force -Path "include" | Out-Null
New-Item -ItemType Directory -Force -Path "reports" | Out-Null

Write-Host "Fetching official cJSON parser sources from GitHub..." -ForegroundColor Cyan
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h" -OutFile "include\cJSON.h"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c" -OutFile "src\cJSON.c"

# Maintain empty reports folder in Git
if (!(Test-Path "reports\.gitkeep")) {
    New-Item -ItemType File -Path "reports\.gitkeep" | Out-Null
}

Write-Host "Project bootstrap complete." -ForegroundColor Green
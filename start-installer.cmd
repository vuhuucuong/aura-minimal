@echo off
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo Node.js is required to start this local website.
  pause
  exit /b 1
)
echo Open http://localhost:8080 in Chrome or Edge.
node server.cjs
pause

@echo off
title Mosaic Admin Panel
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0windows\open-admin-panel.ps1"
if errorlevel 1 pause

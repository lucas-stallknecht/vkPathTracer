@echo off
setlocal enabledelayedexpansion
set ASSETS_DIR=assets
set BUILD_ASSETS_DIR=cmake-build-debug\assets

@REM Create the output directory if it doesn't exist
if not exist "%BUILD_ASSETS_DIR%" (
    mkdir "%BUILD_ASSETS_DIR%"
)

@REM Copy all files and directories from ASSETS_DIR to BUILD_ASSETS_DIR
xcopy "%ASSETS_DIR%\*" "%BUILD_ASSETS_DIR%\" /E /C /I /Y

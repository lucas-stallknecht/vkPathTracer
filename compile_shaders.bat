@echo off
setlocal enabledelayedexpansion
set GLSLC="C:/VulkanSDK/1.3.283.0/Bin/glslc.exe"
set SHADER_DIR=shaders
set COMPILED_SHADER_BUILD_DIR=cmake-build-debug\shaders

@REM Create the output directory if it doesn't exist
if not exist "%COMPILED_SHADER_BUILD_DIR%" (
    mkdir "%COMPILED_SHADER_BUILD_DIR%"
)

for %%f in (%SHADER_DIR%\*.comp) do (
    set OUTPUT_FILE="%COMPILED_SHADER_BUILD_DIR%\%%~nxf.spv"
    echo Compiling %%f to !OUTPUT_FILE!
    %GLSLC% %%f -o !OUTPUT_FILE!
)

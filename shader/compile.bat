@echo off
setlocal
pushd "%~dp0" || exit /b 1

set "GLSLC=glslc"
if exist "%~dp0glslc.exe" set "GLSLC=%~dp0glslc.exe"

if not exist "%GLSLC%" (
    where glslc >nul 2>nul
    if errorlevel 1 (
        echo Error: glslc was not found. Install the Vulkan SDK or put glslc on PATH.
        pause
        popd
        exit /b 1
    )
)

set "COUNT=0"
set "FAILED=0"
for %%F in (*.vert *.tesc *.tese *.geom *.frag *.comp *.task *.mesh *.rgen *.rint *.rahit *.rchit *.rmiss *.rcall) do if exist "%%F" call :compile "%%F"

if "%COUNT%"=="0" (
    echo No shader source files found.
    pause
    popd
    exit /b 0
)

if "%FAILED%"=="1" (
    echo One or more shaders failed to compile.
    pause
    popd
    exit /b 1
)

echo Successfully compiled %COUNT% shader(s).
pause
popd
exit /b 0

:compile
set /a COUNT+=1
echo Compiling %~1
"%GLSLC%" "%~1" -o "%~1.spv"
if errorlevel 1 set "FAILED=1"
exit /b 0

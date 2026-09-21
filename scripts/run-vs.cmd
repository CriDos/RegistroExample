@echo off
rem Loads the MSVC environment (found via vswhere, any VS version/install dir),
rem adds the Qt bin dir from CMAKE_PREFIX_PATH to PATH, then runs the rest of
rem the command line. Used by .vscode/tasks.json so task argv stays free of
rem spaces (VS Code re-quotes space-containing arguments and breaks the
rem classic ""..."" pattern).
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
)
if not defined VS_PATH set "VS_PATH=%VSINSTALLDIR%"
if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    echo [run-vs] MSVC Build Tools not found. Install VS with the C++ workload, or set VSINSTALLDIR. 1>&2
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
rem Fallback: when CMAKE_PREFIX_PATH is not in the calling environment (e.g. VS
rem Code started from a plain shell), derive the Qt kit from the CMake cache
rem (Qt6_DIR .../lib/cmake/Qt6) so the repo stays machine-agnostic.
rem NOTE: the transform chain lives OUTSIDE any if-block - cmd substitutes
rem every %var% when a block is parsed, so in-block sets would clobber each other.
if not defined CMAKE_PREFIX_PATH (
    if exist "%~dp0..\build\CMakeCache.txt" (
        for /f "usebackq tokens=2 delims==" %%p in (`findstr /r /c:"^Qt6_DIR:PATH=" "%~dp0..\build\CMakeCache.txt" 2^>nul`) do set "CMAKE_PREFIX_PATH=%%p"
    )
)
set "CMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH:/lib/cmake/Qt6=%"
set "CMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH:/=\%"
for /f "delims=;" %%a in ("%CMAKE_PREFIX_PATH%") do set "CMAKE_PREFIX_PATH=%%a"
if not defined CMAKE_PREFIX_PATH (
    echo [run-vs] warning: CMAKE_PREFIX_PATH not set, Qt bin dir was not added to PATH 1>&2
) else (
    set "PATH=%CMAKE_PREFIX_PATH%\bin;%PATH%"
)
%*
exit /b %ERRORLEVEL%
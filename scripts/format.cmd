@echo off
rem Formats all project sources. clang-format discovery: 1) LLVM tools shipped
rem with VS Build Tools (vswhere), 2) clang-format bundled in the C/C++ VS Code
rem extension (ms-vscode.cpptools), 3) PATH. qmlformat (QML/JS) is taken from
rem PATH - run this through scripts\run-vs.cmd (or any shell with the Qt kit
rem bin dir on PATH) so qmlformat.exe is found; QML formatting is skipped
rem otherwise. Run via the "Format sources" task or directly.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "CF="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
    if exist "%VS_PATH%\VC\Tools\Llvm\x64\bin\clang-format.exe" set "CF=%VS_PATH%\VC\Tools\Llvm\x64\bin\clang-format.exe"
)
if not defined CF (
    for /f "delims=" %%i in ('dir /b "%USERPROFILE%\.vscode\extensions\ms-vscode.cpptools-*" 2^>nul') do (
        if exist "%USERPROFILE%\.vscode\extensions\%%i\LLVM\bin\clang-format.exe" set "CF=%USERPROFILE%\.vscode\extensions\%%i\LLVM\bin\clang-format.exe"
    )
)
if not defined CF set "CF=clang-format"
for %%f in ("%~dp0..\src\server\*.cpp" "%~dp0..\src\server\*.h" "%~dp0..\src\client\*.cpp" "%~dp0..\src\client\*.h" "%~dp0..\src\common\*.cpp" "%~dp0..\src\common\*.h" "%~dp0..\tests\*.cpp") do (
    if exist "%%f" "%CF%" -i --style=file "%%f"
)
where qmlformat >nul 2>&1
if errorlevel 1 (
    echo [format] qmlformat not found on PATH - skipping QML ^(run via run-vs.cmd to include it^) 1>&2
    exit /b 0
)
for %%f in ("%~dp0..\src\client\qml\*.qml" "%~dp0..\src\client\qml\*.js") do (
    if exist "%%f" qmlformat -i "%%f"
)
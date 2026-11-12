@echo off
setlocal

REM Builds hcc.exe (Raven's HexenC compiler with jsHexen2's -ot) with VS 2022.
REM Output: release\hcc.exe

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Is Visual Studio 2022 installed?
    exit /b 1
)
for /f "delims=" %%i in ('"%VSWHERE%" -latest -property installationPath') do set "VSDIR=%%i"
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1

pushd "%~dp0"
if not exist release mkdir release
cl /nologo /W3 /O2 /D WIN32 /D NDEBUG /D _CONSOLE /D _CRT_SECURE_NO_WARNINGS ^
    hcc.C PR_COMP.C PR_LEX.C CMDLIB.C expr.c stmt.c ^
    /Fo"release\\" /Fe"release\hcc.exe"
if errorlevel 1 (
    echo BUILD FAILED
    popd
    exit /b 1
)
echo BUILD SUCCEEDED: release\hcc.exe
popd

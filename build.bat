@echo off
setlocal

:: Try to find Visual Studio build tools
where cl >nul 2>&1
if %errorlevel% equ 0 goto :compile

if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    goto :compile
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    goto :compile
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    goto :compile
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    goto :compile
)

echo ERRO: Visual Studio ou Build Tools nao encontrado.
echo Instale o "Build Tools for Visual Studio" em:
echo   https://visualstudio.microsoft.com/downloads/
exit /b 1

:compile

if not exist build\release mkdir build\release
echo Compilando nvEncodeAPI64.dll [RELEASE] ...
cl /nologo /W4 /O2 /LD ^
   /Fe:build\release\nvEncodeAPI64.dll ^
   /Fo:build\release\ ^
   src\shim.c ^
   /I src ^
   /link /DEF:nvEncodeAPI64.def /OUT:build\release\nvEncodeAPI64.dll
if %errorlevel% neq 0 (
    echo ERRO na compilacao [release]!
    exit /b 1
)

if not exist build\debug mkdir build\debug
echo.
echo Compilando nvEncodeAPI64.dll [DEBUG] ...
cl /nologo /W4 /O2 /LD /DSHIM_DEBUG ^
   /Fe:build\debug\nvEncodeAPI64.dll ^
   /Fo:build\debug\ ^
   src\shim.c ^
   /I src ^
   /link /DEF:nvEncodeAPI64.def /OUT:build\debug\nvEncodeAPI64.dll
if %errorlevel% neq 0 (
    echo ERRO na compilacao [debug]!
    exit /b 1
)

echo.
echo Build OK!
echo.
echo   build\release\nvEncodeAPI64.dll  (sem logging)
echo   build\debug\nvEncodeAPI64.dll    (com logging)

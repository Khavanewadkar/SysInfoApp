@echo off
if exist "sysinfo.exe" del "sysinfo.exe"

where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Compiling resources...
    windres sysinfo.rc -O coff -o sysinfo_res.o
    echo Compiling with GCC...
    gcc sysinfo.c sysinfo_res.o -o sysinfo.exe -lws2_32 -lwinmm -liphlpapi
    if exist "sysinfo.exe" (
        echo Compilation successful!
        echo.
        start /b sysinfo.exe
    ) else (
        echo Compilation failed.
    )
    exit /b
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Compiling with MSVC...
    cl sysinfo.c /Fe:sysinfo.exe
    if exist "sysinfo.exe" (
        echo Compilation successful!
        echo.
        sysinfo.exe
    ) else (
        echo Compilation failed.
    )
    exit /b
)

echo No compiler found (gcc or cl). Please install a C compiler or use Developer Command Prompt.
pause

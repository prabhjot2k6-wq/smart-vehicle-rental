@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto error
cmake --build build --target rental_api
if errorlevel 1 goto error
if not exist data mkdir data
build\rental_api.exe data\rental_data.txt
goto end
:error
echo.
echo Build failed. Check that CMake, a recent MinGW-w64 C++17 compiler, and Boost headers are installed and on PATH.
pause
:end
endlocal

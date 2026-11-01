@echo off
title Hospital Management System
cd /d "%~dp0"

REM Step 1: download httplib.h if it is missing
if not exist httplib.h (
    echo Downloading httplib.h ...
    powershell -Command "Invoke-WebRequest -Uri https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -OutFile httplib.h"
)

REM Step 2: compile the C++ backend (needs g++ / MinGW)
echo Compiling...
g++ -std=c++17 -D_WIN32_WINNT=0x0A00 main.cpp server.cpp -o server.exe -lws2_32
if errorlevel 1 (
    echo.
    echo Build failed. Make sure g++ ^(MinGW^) is installed and in PATH.
    pause
    exit /b 1
)

REM Step 3: open the browser and start the server
start http://localhost:8080
server.exe
pause

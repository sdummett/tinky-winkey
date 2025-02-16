@echo off
setlocal

echo ===========================
echo ==== Building Keylogger ===
echo ===========================
cd keylogger
nmake
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to build keylogger.
    exit /b %ERRORLEVEL%
)
copy /Y winkey.exe ..\
cd ..

echo ===========================
echo ==== Building Service =====
echo ===========================
cd service
nmake
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to build service.
    exit /b %ERRORLEVEL%
)
copy /Y service.exe ..\
cd ..

echo ===========================
echo ==== Build Successful! ====
echo ===========================
exit /b 0

@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

set "PYTHON_CMD="
where py >nul 2>nul
if not errorlevel 1 set "PYTHON_CMD=py -3"

if not defined PYTHON_CMD (
    where python >nul 2>nul
    if not errorlevel 1 set "PYTHON_CMD=python"
)

if not defined PYTHON_CMD (
    echo 未找到 Python，请先安装 Python 3。
    exit /b 2
)

%PYTHON_CMD% -c "import serial" >nul 2>nul
if errorlevel 1 (
    echo 缺少 pyserial，请先运行：
    echo   %PYTHON_CMD% -m pip install pyserial
    exit /b 2
)

%PYTHON_CMD% "%~dp0serial_motor_test.py" %*
exit /b %errorlevel%

@echo off

chcp 65001

net session >nul 2>&1
if %errorLevel% NEQ 0 (
    echo 관리자 권한으로 다시 실행합니다
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set DRIVER_NAME=MobinogiAutoClick
set DRIVER_SYS_PATH=D:\MobinogiAutoClick\x64\Debug\MobinogiAutoClick.sys

sc stop %DRIVER_NAME% >nul 2>&1
sc delete %DRIVER_NAME% >nul 2>&1
sc create %DRIVER_NAME% type= kernel binPath= "%DRIVER_SYS_PATH%"
sc start %DRIVER_NAME%

echo [✓] 드라이버 로드 완료!
pause

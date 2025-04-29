@echo off

chcp 65001

net session >nul 2>&1
if %errorLevel% NEQ 0 (
    echo 관리자 권한으로 다시 실행합니다
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set DRIVER_NAME=MobinogiAutoClick

sc stop %DRIVER_NAME%
sc delete %DRIVER_NAME%

echo [✓] 드라이버 제거 완료
pause

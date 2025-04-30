@echo off
echo [•] PowerShell 인증서 생성 스크립트를 실행합니다...
powershell -ExecutionPolicy Bypass -File "%~dp0createCert.ps1"

if exist "%~dp0MyDriverCert.pfx" (
    echo [✓] 인증서 생성 성공!
) else (
    echo [X] 실패! 관리자 권한 또는 정책 문제 확인 필요
)
pause
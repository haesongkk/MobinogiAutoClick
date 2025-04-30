@echo off

chcp 65001
setlocal

:: 파일 경로 설정
set SYS_FILE=..\x64\Release\MouseDriver.sys
set PFX_FILE=..\x64\Release\MyDriverCert.pfx
set PFX_PASS=mypassword

set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
echo [•] 드라이버에 정식 서명 추가 중...

%SIGNTOOL% sign ^
  /f "%PFX_FILE%" ^
  /p "%PFX_PASS%" ^
  /fd SHA256 ^
  /tr http://timestamp.digicert.com ^
  /td SHA256 ^
  "%SYS_FILE%"

if %errorlevel% equ 0 (
    echo [✓] 서명 성공: %SYS_FILE%
) else (
    echo [X] 서명 실패! 인증서 또는 파일 경로 확인 필요
)

pause

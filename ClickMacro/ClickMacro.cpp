#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "wintrust.lib")

#include <windows.h>
#include <wincrypt.h>
#include <winsvc.h>
#include <stdio.h>
#include <clocale>

#define _CRT_INTERNAL_NONSTDC_NAMES 1
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601  // 최소 Windows 7 이상

#include <wintrust.h>
#include <softpub.h>
#include <mscat.h>
#include <tchar.h>


BOOL RegisterCertificate(const wchar_t* cerPath)
{
    // .cer -> hFile 
    HANDLE hFile = CreateFileW(cerPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        wprintf(L"[ERROR] 인증서 파일 열기 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    // fFile -> buffer
    DWORD size = GetFileSize(hFile, NULL);
    BYTE* buffer = (BYTE*)malloc(size);
    DWORD read;
    ReadFile(hFile, buffer, size, &read, NULL);
    CloseHandle(hFile);


    PCCERT_CONTEXT cert = CertCreateCertificateContext(X509_ASN_ENCODING, buffer, size);

    free(buffer);
    if (!cert) 
    {
        wprintf(L"[ERROR] 인증서 컨텍스트 생성 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    // cert 등록
    const wchar_t* names[2] = { L"TrustedPublisher", L"Root" };
    for (int i = 0; i < 2; ++i) 
    {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL,
            CERT_SYSTEM_STORE_LOCAL_MACHINE, names[i]);

        if (!store)
        {
            wprintf(L"[ERROR] 인증서 저장소 %s 열기 실패\n", names[i]);
            CertFreeCertificateContext(cert);
            return FALSE;
        }

        BOOL ok = CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_REPLACE_EXISTING, NULL);
        if (!ok)
        {
            wprintf(L"[ERROR] 인증서 등록 실패 (%lu)\n", GetLastError());
            return FALSE;
        }

        wprintf(L"[DEBUG] 저장소 %s 등록 완료\n", names[i]);

        CertCloseStore(store, 0);
    }
    CertFreeCertificateContext(cert);
    wprintf(L"[DEBUG] 인증서 등록 완료\n");
    return TRUE;
}

void DeleteCertificatesBySubject(const wchar_t* subjectCN)
{
    const wchar_t* names[2] = { L"TrustedPublisher", L"Root" };
    for (int i = 0; i < 2; ++i)
    {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL,
            CERT_SYSTEM_STORE_LOCAL_MACHINE, names[i]);

        if (!store)
        {
            wprintf(L"[ERROR] 인증서 저장소 %s 열기 실패\n", names[i]);
            continue;
        }

        PCCERT_CONTEXT cert = NULL;
        int removed = 0;

        while ((cert = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
            CERT_FIND_SUBJECT_STR, subjectCN, cert)))
        {
            PCCERT_CONTEXT toDelete = CertDuplicateCertificateContext(cert);
            CertDeleteCertificateFromStore(toDelete);
        }

        wprintf(L"[DEBUG] %s에서 인증서 삭제 완료\n", names[i]);
        CertCloseStore(store, 0);
    }
}

#define COLOR_HIGHLIGHT FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define COLOR_NORMAL FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE

void ListAllCertificates(const wchar_t* subjectCN) 
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    const wchar_t* names[2] = { L"TrustedPublisher", L"Root" };
    for (int i = 0; i < 2; ++i) 
    {
        HCERTSTORE store;
        store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL,
            CERT_SYSTEM_STORE_LOCAL_MACHINE, names[i]);
        if (!store)
        {
            wprintf(L"[ERROR] 인증서 저장소 %s 열기 실패\n", names[i]);
            return;
        }

        wprintf(L"\n[•] 저장소: %s\n", names[i]);

        PCCERT_CONTEXT cert = NULL;
        // 인증서 이름을 하나씩 출력한다
        while ((cert = CertEnumCertificatesInStore(store, cert)))
        {
            DWORD size = CertNameToStrW(
                X509_ASN_ENCODING, 
                &cert->pCertInfo->Subject, 
                CERT_SIMPLE_NAME_STR, 
                NULL, 0);
            wchar_t* name = (wchar_t*)malloc(size * sizeof(wchar_t));
            CertNameToStrW(
                X509_ASN_ENCODING, 
                &cert->pCertInfo->Subject,
                CERT_SIMPLE_NAME_STR, 
                name, size);

            if (wcsstr(name, subjectCN)) 
            {
                SetConsoleTextAttribute(hConsole, COLOR_HIGHLIGHT);
                wprintf(L" %s\n", name);
                SetConsoleTextAttribute(hConsole, COLOR_NORMAL);
            }
            else wprintf(L"%s\n", name);

            free(name);
        }

        CertCloseStore(store, 0);
    }
}


// 드라이버 등록
BOOL InstallAndStartDriver(const wchar_t* serviceName, const wchar_t* sysPath)
{
    wchar_t absolutePath[200];
    DWORD result = GetFullPathNameW(sysPath, 200, absolutePath, NULL);
    if (result == 0 || result >= 200)
    {
        wprintf(L"[-] GetFullPathNameW 실패 (%lu)\n", GetLastError());
        return FALSE;
    }
    wprintf(L"[DEBUG] sys 절대 경로: %s\n", absolutePath);
    getchar();

    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        wprintf(L"[DEBUG] OpenSCManager 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    // .sys -> hService
    SC_HANDLE hService = CreateServiceW(
        hSCM,
        serviceName,
        serviceName,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        absolutePath,
        NULL, NULL, NULL, NULL, NULL);

    if (!hService)
    {
        DWORD err = GetLastError();
        wprintf(L"[DEBUG] CreateService 실패 (%lu)\n", err);  // ← 이걸 무조건 넣어야 해요

        if (err == ERROR_SERVICE_EXISTS)
        {

            hService = OpenServiceW(hSCM, serviceName, SERVICE_ALL_ACCESS);
            if (!hService) {
                wprintf(L"[DEBUG] OpenService 실패 (%lu)\n", GetLastError());
                CloseServiceHandle(hSCM);
                return FALSE;
            }
        }
        else
        {
            wprintf(L"[DEBUG] CreateService 실패 (%lu)\n", err);
            CloseServiceHandle(hSCM);
            return FALSE;
        }
    }

    BOOL started = StartServiceW(hService, 0, NULL);
    if (!started)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING)
        {
            wprintf(L"[!] 드라이버는 이미 실행 중입니다.\n");
        }
        else
        {
            wprintf(L"[-] StartService 실패 (%lu)\n", err);
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return FALSE;
        }
    }
    else 
    {
        wprintf(L"[✓] 드라이버 시작 성공\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return TRUE;
}

BOOL StopAndRemoveDriver(const wchar_t* serviceName)
{
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        wprintf(L"[-] OpenSCManager 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, serviceName, SERVICE_ALL_ACCESS);
    if (!hService) {
        wprintf(L"[-] OpenService 실패 (%lu)\n", GetLastError());
        CloseServiceHandle(hSCM);
        return FALSE;
    }

    SERVICE_STATUS status;
    ControlService(hService, SERVICE_CONTROL_STOP, &status);
    DeleteService(hService);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    wprintf(L"[✓] 드라이버 중지 및 삭제 완료\n");
    return TRUE;
}

void ListAllDrivers(const wchar_t* myDriverName)
{
    ENUM_SERVICE_STATUS services[1024];
    DWORD bytesNeeded, servicesReturned, resumeHandle = 0;

    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCM) {
        wprintf(L"[-] OpenSCManager 실패 (%lu)\n", GetLastError());
        return;
    }

    BOOL success = EnumServicesStatusW(
        hSCM,
        SERVICE_DRIVER,
        SERVICE_STATE_ALL,
        services,
        sizeof(services),
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle
    );

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!success) {
        wprintf(L"[-] 드라이버 열거 실패 (%lu)\n", GetLastError());
        CloseServiceHandle(hSCM);
        return;
    }

    wprintf(L"[•] 현재 실행 중인 커널 드라이버 목록:\n");

    for (DWORD i = 0; i < servicesReturned; ++i) {
        const wchar_t* svcName = services[i].lpServiceName;

        if (_wcsicmp(svcName, myDriverName) == 0) {
            SetConsoleTextAttribute(hConsole, COLOR_HIGHLIGHT);
            wprintf(L"  [RUNNING] %s  <-- 내 드라이버\n", svcName);
            SetConsoleTextAttribute(hConsole, COLOR_NORMAL);
        }
        else {
            wprintf(L"  [RUNNING] %s\n", svcName);
        }
    }

    CloseServiceHandle(hSCM);
}


BOOL PrintSha1OfSysSignature(const wchar_t* sysPath)
{
    WINTRUST_FILE_INFO fileInfo = { 0 };
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = sysPath;

    WINTRUST_DATA winTrust = { 0 };
    winTrust.cbStruct = sizeof(winTrust);
    winTrust.dwUIChoice = WTD_UI_NONE;
    winTrust.fdwRevocationChecks = WTD_REVOKE_NONE;
    winTrust.dwUnionChoice = WTD_CHOICE_FILE;
    winTrust.pFile = &fileInfo;
    winTrust.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrust.dwProvFlags = WTD_SAFER_FLAG;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    LONG result = WinVerifyTrust(NULL, &policyGUID, &winTrust);
    if (result != 0) {
        wprintf(L"[-] WinVerifyTrust 실패 (%lu)\n", result);
        return FALSE;
    }

    CRYPT_PROVIDER_DATA* provData = WTHelperProvDataFromStateData(winTrust.hWVTStateData);
    if (!provData) {
        wprintf(L"[-] provData 불러오기 실패\n");
        return FALSE;
    }

    CRYPT_PROVIDER_SGNR* sgnr = WTHelperGetProvSignerFromChain(provData, 0, FALSE, 0);
    if (!sgnr || !sgnr->pChainContext || sgnr->pChainContext->cChain == 0)
    {
        wprintf(L"[-] 서명 인증서 체인 없음\n");
        return FALSE;
    }

    // 실제 서명된 인증서
    PCCERT_CONTEXT cert = sgnr->pChainContext->rgpChain[0]->rgpElement[0]->pCertContext;

    BYTE hash[20];
    DWORD hashSize = sizeof(hash);
    if (CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &hashSize)) {
        wprintf(L"[✓] .sys 서명 인증서 SHA1: ");
        for (DWORD i = 0; i < hashSize; i++) printf("%02X", hash[i]);
        wprintf(L"\n");
    }
    else {
        wprintf(L"[-] SHA1 추출 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    winTrust.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &policyGUID, &winTrust);
    return TRUE;
}



BOOL PrintSha1OfCerFile(const wchar_t* cerPath)
{
    HANDLE hFile = CreateFileW(cerPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        wprintf(L"[-] .cer 파일 열기 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    DWORD size = GetFileSize(hFile, NULL);
    BYTE* buffer = (BYTE*)malloc(size);
    DWORD read;
    ReadFile(hFile, buffer, size, &read, NULL);
    CloseHandle(hFile);

    PCCERT_CONTEXT cert = CertCreateCertificateContext(X509_ASN_ENCODING, buffer, size);
    free(buffer);
    if (!cert) {
        wprintf(L"[-] CertCreateCertificateContext 실패 (%lu)\n", GetLastError());
        return FALSE;
    }

    BYTE hash[20];
    DWORD hashSize = sizeof(hash);
    if (CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &hashSize)) {
        wprintf(L"[✓] .cer 파일 SHA1: ");
        for (DWORD i = 0; i < hashSize; i++) printf("%02X", hash[i]);
        wprintf(L"\n");
    }

    CertFreeCertificateContext(cert);
    return TRUE;
}


int main()
{
    std::setlocale(LC_ALL, "Korean_Korea.UTF-8");


    RegisterCertificate(L"..\\x64\\Release\\MyDriverCert.cer");
    PrintSha1OfSysSignature(L"D:\\MobinogiAutoClick\\x64\\Release\\MouseDriver.sys");
    PrintSha1OfCerFile(L"D:\\MobinogiAutoClick\\x64\\Release\\MyDriverCert.cer");
    getchar();
    
    ListAllCertificates(L"MyDriverCert");
    getchar();
    
    InstallAndStartDriver(L"MouseDriver", L"..\\x64\\Release\\MouseDriver.sys");
    getchar();
    
    ListAllDrivers(L"MouseDriver");
    getchar();
    
    StopAndRemoveDriver(L"MouseDriver");
    getchar();
    
    ListAllDrivers(L"MouseDriver");
    getchar();
    
    DeleteCertificatesBySubject(L"MyDriverCert");
    getchar();
    
    ListAllCertificates(L"MyDriverCert");
    getchar();
}




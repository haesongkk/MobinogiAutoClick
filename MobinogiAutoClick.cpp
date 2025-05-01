#include <windows.h>
#include <stdio.h>
#include <string>

bool IsRunningAsAdmin() 
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

bool RunAsAdmin()
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);

    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = szPath;
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteEx(&sei))
    {
        MessageBox(NULL, L"������ ���� ���࿡ �����߽��ϴ�.", L"����", MB_OK);
        return true;
    }

    return false; 
}

void DisableQuickEditMode()
{
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hConsole, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    mode |= ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(hConsole, mode);
}

// ==================================================
//                      ��   ��
// ==================================================
POINT mousePos;
enum KEYSTATE { NONE, TAB, PUSH, AWAY };
enum KEY { LB, ONE, TWO, END };
int keyValue[END] = { VK_LBUTTON ,'1','2' };
KEYSTATE keyState[END] = { NONE,NONE,NONE };
bool prevDown[END] = { false,false,false };
void UpdateInput()
{
    // ���콺
    GetCursorPos(&mousePos);
    
    // Ű����
    for (int i = 0; i < END; i++)
    {
        bool bDown = GetAsyncKeyState(keyValue[i]) & 0x8000;
        if (prevDown[i] && bDown) keyState[i] = PUSH;
        else if (prevDown[i] && !bDown) keyState[i] = AWAY;
        else if (!prevDown[i] && bDown) keyState[i] = TAB;
        else if (!prevDown[i] && !bDown) keyState[i] = NONE;
        prevDown[i] = bDown;
    }
}

// ==================================================
//                      Ÿ �� ��
// ==================================================
DWORD prevTick;
DWORD GetDeltaTime()
{
    DWORD curTick = GetTickCount();
    DWORD delta = curTick - prevTick;
    prevTick = curTick;
    return delta;
}

// ==================================================
//                      ��   ��
// ==================================================
enum APPSTATE { DEFAULT, RECORD, PLAYBACK, };
APPSTATE appState = DEFAULT;
int recordIndex = 0;
int playbackIndex = 0;
const char* intro[3] = {
    "��ȭ1 ���2\n",
    "��ȭ ��... ��ȭ �ߴ�1\n",
    "��� ��... ��� �ߴ�2\n" 
};
void SetAppState(APPSTATE _appState)
{
    appState = _appState;
    printf(intro[_appState]);
    playbackIndex = 0;
    if (_appState == RECORD) recordIndex = 0;
}

// ==================================================
//                      ��   ��
// ==================================================
#define FPS 30
#define TIMELIMIT 60
#define MAXDATA (FPS*TIMELIMIT)

struct MOUSEDATA { POINT pos; KEYSTATE state; };
MOUSEDATA mouseData[MAXDATA];

void Record() 
{
    mouseData[recordIndex] = { mousePos,keyState[LB] };
    if (++recordIndex >= MAXDATA) SetAppState(DEFAULT);
    if (keyState[ONE] == TAB) SetAppState(DEFAULT);
}

void PlayBack() 
{
    POINT pos = mouseData[playbackIndex].pos;
    KEYSTATE state = mouseData[playbackIndex].state;
    SetCursorPos(pos.x, pos.y);
    if (state == TAB) mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    if (state == AWAY) mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

    if (++playbackIndex >= recordIndex) SetAppState(DEFAULT);//playbackIndex = 0;
    if (keyState[TWO] == TAB) SetAppState(DEFAULT);
}

void AppDefault() 
{
    if (keyState[ONE] == TAB) SetAppState(RECORD);
    if (keyState[TWO] == TAB) SetAppState(PLAYBACK);
}

void (*UpdateApp[3])() = 
{ 
    AppDefault, 
    Record, 
    PlayBack 
};




int main() 
{
    if (!IsRunningAsAdmin()) return RunAsAdmin();
    DisableQuickEditMode();
    SetAppState(DEFAULT);
    DWORD elapsedTime = 0;
    DWORD interval = 1000.0f / FPS;
    while (true)
    {
        elapsedTime += GetDeltaTime();
        while (elapsedTime >= interval)
        {
            elapsedTime -= interval;
            UpdateInput();
            UpdateApp[appState]();
        }
    }
    return 0;
}

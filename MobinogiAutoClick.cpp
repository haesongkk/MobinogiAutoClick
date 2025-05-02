#include <windows.h>
#include <string>
#include <vector>

// ==============================================================
// 프로그램 실행
// ==============================================================

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
        MessageBox(NULL, L"관리자 권한 실행에 실패했습니다.", L"실패", MB_OK);
        return true;
    }

    return false;
}

// ==============================================================
// 윈도우 생성
// ==============================================================

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

HWND g_overlay = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hBitmap = nullptr;
int g_width = 0;
int g_height = 0;

bool CreateOverlayWindow(const wchar_t* _targetWndTitle)
{
    HWND target = FindWindow(NULL, _targetWndTitle);
    if (!target || !IsWindow(target))
    {
        MessageBox(NULL,
            L"모비노기를 찾는데 실패했습니다",
            L"오류",
            MB_ICONERROR);
        return false;
    }

    RECT rect;
    GetWindowRect(target, &rect);
    g_width = rect.right - rect.left;
    g_height = rect.bottom - rect.top;

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"OverlayClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        L"OverlayClass", L"",
        WS_POPUP,
        rect.left, rect.top, g_width, g_height,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    g_overlay = hwnd;
    return true;
}

void Text(const wchar_t* text, int fontSize=18, COLORREF color=RGB(255,0,0)) 
{
    if (!g_overlay) return;

    HDC hdcScreen = GetDC(NULL);
    if (!g_hdcMem) g_hdcMem = CreateCompatibleDC(hdcScreen);
    if (g_hBitmap) DeleteObject(g_hBitmap);
    g_hBitmap = CreateCompatibleBitmap(hdcScreen, g_width, g_height);
    SelectObject(g_hdcMem, g_hBitmap);

    RECT rc = { 0, 0, g_width, g_height };
    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); // 완전 투명

    // 폰트 생성
    HFONT hFont = CreateFontW(
        fontSize, 0, 0, 0, 
        FW_BOLD, 
        FALSE, FALSE, FALSE,
        HANGEUL_CHARSET, 
        OUT_DEFAULT_PRECIS, 
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, 
        DEFAULT_PITCH | FF_DONTCARE,
        L"맑은 고딕");

    SelectObject(g_hdcMem, hFont);
    SetBkMode(g_hdcMem, TRANSPARENT);
    SetTextColor(g_hdcMem, color);
    DrawText(g_hdcMem, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hFont);

    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { g_width, g_height };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_overlay, hdcScreen, NULL, &sizeWnd, g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    ReleaseDC(NULL, hdcScreen);
}

// ==============================================================
// 입력
// ==============================================================

enum KEYSTATE {
    NONE,
    TAB,
    PUSH,
    AWAY
};
enum KEY {
    ESC,
    LBUTTON,
    RBUTTON,
    LCTRL,
    LALT,
    SPACE,
    ETC,
    END
};
int keyValue[KEY::END] = {
    VK_ESCAPE,
    VK_LBUTTON,
    VK_RBUTTON,
    VK_LCONTROL,
    VK_LMENU,
    VK_SPACE,
    VK_OEM_3
};
KEYSTATE keyState[END] = { NONE, };
bool prevDown[END] = { false, };
POINT mousePos;

void UpdateInput()
{
    // 마우스
    GetCursorPos(&mousePos);

    // 키보드
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

// ==============================================================
// 이미지 처리
// ==============================================================

#define FPS 30
#define TIMELIMIT 60
#define MAXDATA (FPS*TIMELIMIT)
#define RECORDINTERVAL (1000.0f/FPS)
#define PLAYINERTVAL (RECORDINTERVAL/2)

#define QUIT ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) ? (ExitProcess(0), true) :false)

struct MOUSEDATA { POINT pos; KEYSTATE state; };
int recordIndex = 0;
int playbackIndex = 0;
RECT detectBox;

std::vector< MOUSEDATA> CreateMouseMacro()
{
    Text(L"CTRL 누른 상태로 녹화하세요");

    std::vector<MOUSEDATA> macro;
    while (keyState[LCTRL] != TAB && !QUIT)
        UpdateInput();
    while (keyState[LCTRL] != AWAY && !QUIT)
    {
        UpdateInput();
        macro.push_back({ mousePos,keyState[LBUTTON] });
        Sleep(RECORDINTERVAL);
    }
    return macro;
}

void PlayMouseMacro(std::vector< MOUSEDATA> _macro)
{
    Text(L"매크로 재생 중ㅁㅁㅁㅁ");
    for (auto mouseData : _macro)
    {
        if (QUIT) break;
        SetCursorPos(mouseData.pos.x, mouseData.pos.y);
        if (mouseData.state == TAB) mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        if (mouseData.state == AWAY) mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        Sleep(PLAYINERTVAL);
    }
}

RECT CreateDetectBox()
{
    Text(L"우클릭을 드래그하여 혅재 퀘스트 ui 위치를 알려주세요");

    while (keyState[RBUTTON] != TAB && !QUIT) UpdateInput();
    POINT start;
    GetCursorPos(&start);

    while (keyState[RBUTTON] != AWAY && !QUIT) UpdateInput();
    POINT end;
    GetCursorPos(&end);
    RECT box;

    box.left = min(start.x, end.x);
    box.top = min(start.y, end.y);
    box.right = max(start.x, end.x);
    box.bottom = max(start.y, end.y);

    return box;
}

void ShowDetectBox(RECT _box)
{
    HDC hdc = GetDC(NULL);
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    // 테두리 그리기
    Rectangle(hdc, _box.left, _box.top, _box.right, _box.bottom);
    //Rectangle(hdc, 100,100,1000,1000);

    // 리소스 정리
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(hPen);
    ReleaseDC(NULL, hdc);
}
void ShowErrorBox(const std::wstring& msg)
{
    MessageBoxW(NULL, msg.c_str(), L"Capture Error", MB_ICONERROR);
}
std::vector<uint8_t> CaptureGray(RECT box)
{

    int width = box.right - box.left;
    int height = box.bottom - box.top;

    if (width <= 0 || height <= 0) {
        ShowErrorBox(L"캡처 영역이 잘못되었습니다.");
        return {};
    }

    HDC hScreen = GetDC(NULL);

    if (!hScreen) {
        ShowErrorBox(L"화면 DC를 얻지 못했습니다.");
        return {};
    }

    HDC hMemDC = CreateCompatibleDC(hScreen);

    if (!hMemDC) {
        ShowErrorBox(L"메모리 DC 생성 실패");
        ReleaseDC(NULL, hScreen);
        return {};
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);


    if (!hBitmap) {
        ShowErrorBox(L"비트맵 생성 실패");
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }

    SelectObject(hMemDC, hBitmap);
    if (!BitBlt(hMemDC, 0, 0, width, height, hScreen, box.left, box.top, SRCCOPY)) 
    {
        ShowErrorBox(L"BitBlt 실패 - 화면 캡처에 실패했습니다.");
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }
    //BitBlt(hMemDC, 0, 0, width, height, hScreen, box.left, box.top, SRCCOPY);

    BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER) };
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int rowStride = ((width * 3 + 3) / 4) * 4; // 각 줄은 4바이트 단위로 정렬됨
    std::vector<uint8_t> raw(rowStride * height * 3);
    if (!GetDIBits(hMemDC, hBitmap, 0, height, raw.data(), &bmi, DIB_RGB_COLORS)) {
        ShowErrorBox(L"GetDIBits 실패 - 비트맵 데이터 획득 실패");
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreen);
        return {};
    }
    //GetDIBits(hMemDC, hBitmap, 0, height, raw.data(), &bmi, DIB_RGB_COLORS);

    std::vector<uint8_t> gray(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src = y * rowStride + x * 3;
            uint8_t b = raw[src + 0];
            uint8_t g = raw[src + 1];
            uint8_t r = raw[src + 2];
            gray[y * width + x] = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
        }
    }

    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreen);
    return gray;
}

#include <cmath>
double CompareImages(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) 
{

    if (a.size() != b.size()) {
        ShowErrorBox(L"입력 벡터의 크기가 다릅니다.");
        return 0.0;
    }
    //if (a.size() != b.size()) return 0.0;

    if (a.empty()) {
        ShowErrorBox(L"입력 벡터가 비어 있습니다.");
        return 0.0;
    }

    double meanA = 0, meanB = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        meanA += a[i];
        meanB += b[i];
    }
    meanA /= a.size();
    meanB /= b.size();

    double num = 0, denomA = 0, denomB = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        double da = a[i] - meanA;
        double db = b[i] - meanB;
        num += da * db;
        denomA += da * da;
        denomB += db * db;
    }
    if (denomA == 0 || denomB == 0) {
        ShowErrorBox(L"이미지가 완전히 균일하여 분산이 0입니다.");
        return 0.0;
    }
    double denom = sqrt(denomA * denomB);
    if (denom < 1e-8 || std::isnan(denom) || std::isinf(denom)) {
        ShowErrorBox(L"유효하지 않은 분모 계산 결과입니다.");
        return 0.0;
    }

    return num / denom;
}

void DetectChange(RECT _box)
{
    Text(L"영역이 바뀔때까지 감지 중입니다");

    std::vector<uint8_t> curGray = CaptureGray(_box);
    std::vector<uint8_t> prevGray = curGray;

    while (CompareImages(prevGray, curGray) > 0.5 && !QUIT)
    {
        ShowDetectBox(_box);

        prevGray = curGray;
        curGray = CaptureGray(_box);
        Sleep(30);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // 관리자 권한이 아니라면 종료 후 관리자 권한으로 재실행
    if (!IsRunningAsAdmin()) return RunAsAdmin();
   
   
    // 모비노기 오버레이 윈도우 생성
    if (!CreateOverlayWindow(L"마비노기 모바일")) return false;

    std::vector< MOUSEDATA> macro;
    macro = CreateMouseMacro();
    PlayMouseMacro(macro);

    RECT box;
    box = CreateDetectBox();
    ShowDetectBox(box);

    while (!QUIT)
    {
        DetectChange(box);
        PlayMouseMacro(macro);
    }

    return 0;

}

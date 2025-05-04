#include <Windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <psapi.h>

#define WM_TRAYICON (WM_USER + 1)

NOTIFYICONDATA nid;
HHOOK mouseHook;
HHOOK keyboardHook;  
HWND hWnd;
HINSTANCE hInst;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam); 

struct FloatingWindowInfo {
    HWND hWnd;
    HFONT hFont;
    COLORREF bgColor;
    COLORREF textColor;
    HBRUSH hBgBrush;
    HPEN hBorderPen;  
};

FloatingWindowInfo g_floatingWindow = { 0 };


void CreateFloatingWindow();
void UpdateFloatingWindow(LPCTSTR text, POINT cursorPos);
void DestroyFloatingWindow();
LRESULT CALLBACK FloatingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void CreateTrayIcon(LPCTSTR title, LPCTSTR iconPath) {
    hInst = GetModuleHandle(NULL);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = _T("TrayWindow");
    RegisterClassEx(&wc);


    hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,  
        _T("TrayWindow"),
        _T(""),
        WS_POPUP,          
        0, 0, 0, 0,        
        NULL,
        NULL,
        hInst,
        NULL
    );

    ShowWindow(hWnd, SW_HIDE);
    UpdateWindow(hWnd);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    _tcscpy_s(nid.szTip, title);
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void CreateFloatingWindow() {
    if (g_floatingWindow.hWnd == NULL) {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = FloatingWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = _T("FloatingWindowClass");
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassEx(&wc);

        g_floatingWindow.hWnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED,
            _T("FloatingWindowClass"),
            _T(""),
            WS_POPUP,
            0, 0, 200, 50,
            NULL,
            NULL,
            wc.hInstance,
            NULL
        );

        g_floatingWindow.bgColor = RGB(255, 255, 255);  
        g_floatingWindow.textColor = RGB(0, 0, 0);      

        g_floatingWindow.hBgBrush = CreateSolidBrush(g_floatingWindow.bgColor);
        g_floatingWindow.hBorderPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));  
        g_floatingWindow.hFont = CreateFont(
            16, 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS,
            _T("Segoe UI")
        );

        SetLayeredWindowAttributes(g_floatingWindow.hWnd, 0, 220, LWA_ALPHA);
    }
}

void UpdateFloatingWindow(LPCTSTR text, POINT cursorPos) {
    if (g_floatingWindow.hWnd == NULL) {
        CreateFloatingWindow();
    }

    HDC hdc = GetDC(g_floatingWindow.hWnd);
    HFONT hOldFont = (HFONT)SelectObject(hdc, g_floatingWindow.hFont);

    RECT textRect = { 0 };
    DrawText(hdc, text, -1, &textRect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX);

    int width = textRect.right + 20 + 2;  
    int height = textRect.bottom + 10 + 2;

    RECT screenRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);

    int newX = cursorPos.x + 16;
    int newY = cursorPos.y + 16;

    if (newX + width > screenRect.right) newX = screenRect.right - width - 5;
    if (newY + height > screenRect.bottom) newY = screenRect.bottom - height - 5;

    SetWindowPos(g_floatingWindow.hWnd, HWND_TOPMOST, newX, newY, width, height, SWP_SHOWWINDOW);
    SetWindowText(g_floatingWindow.hWnd, text);

    SelectObject(hdc, hOldFont);
    ReleaseDC(g_floatingWindow.hWnd, hdc);
    InvalidateRect(g_floatingWindow.hWnd, NULL, TRUE);
}

void DestroyFloatingWindow() {
    if (g_floatingWindow.hWnd) DestroyWindow(g_floatingWindow.hWnd);
    if (g_floatingWindow.hFont) DeleteObject(g_floatingWindow.hFont);
    if (g_floatingWindow.hBgBrush) DeleteObject(g_floatingWindow.hBgBrush);
    if (g_floatingWindow.hBorderPen) DeleteObject(g_floatingWindow.hBorderPen);  
    memset(&g_floatingWindow, 0, sizeof(g_floatingWindow));
}

LRESULT CALLBACK FloatingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HDC hMemDC = CreateCompatibleDC(hdc);
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);

        FillRect(hMemDC, &clientRect, g_floatingWindow.hBgBrush);

        HPEN hOldPen = (HPEN)SelectObject(hMemDC, g_floatingWindow.hBorderPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
        Rectangle(hMemDC, 0, 0, clientRect.right, clientRect.bottom);

        HFONT hOldFont = (HFONT)SelectObject(hMemDC, g_floatingWindow.hFont);
        SetBkMode(hMemDC, TRANSPARENT);
        SetTextColor(hMemDC, g_floatingWindow.textColor);

        TCHAR text[MAX_PATH];
        GetWindowText(hWnd, text, MAX_PATH);
        RECT textRect = clientRect;
        textRect.left += 11;  
        textRect.top += 6;   

        DrawText(hMemDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hMemDC, 0, 0, SRCCOPY);

        SelectObject(hMemDC, hOldFont);
        SelectObject(hMemDC, hOldPen);
        SelectObject(hMemDC, hOldBrush);
        DeleteObject(hBmp);
        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}


void HandleMouseEvent() {

    BOOL ctrlPressed = GetAsyncKeyState(VK_CONTROL) & 0x8000;
    BOOL altPressed = GetAsyncKeyState(VK_MENU) & 0x8000;

    if (ctrlPressed && altPressed) {
        POINT pt;
        GetCursorPos(&pt);
        HWND targetHwnd = WindowFromPoint(pt);

        DWORD processId;
        GetWindowThreadProcessId(targetHwnd, &processId);

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (hProcess) {
            TCHAR processPath[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, NULL, processPath, MAX_PATH)) {
                UpdateFloatingWindow(processPath, pt);
            }
            CloseHandle(hProcess);
        }
    }
    else {
        ShowWindow(g_floatingWindow.hWnd, SW_HIDE);
    }
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKb = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYUP && (pKb->vkCode == VK_CONTROL || pKb->vkCode == VK_MENU)) {
            AnimateWindow(g_floatingWindow.hWnd, 100, AW_HIDE | AW_BLEND);
            ShowWindow(g_floatingWindow.hWnd, SW_HIDE);
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CreateTrayIcon(_T("Process Path Viewer"), _T(""));

    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, hInstance, 0);
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(mouseHook);
    UnhookWindowsHookEx(keyboardHook);
    DestroyFloatingWindow();
    Shell_NotifyIcon(NIM_DELETE, &nid);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TRAYICON && LOWORD(lParam) == WM_RBUTTONUP) {
        PostQuitMessage(0);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        HandleMouseEvent(); 
    }
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}
//by deepseek

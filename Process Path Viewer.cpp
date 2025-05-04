#include <Windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <psapi.h>

#define WM_TRAYICON (WM_USER + 1)

NOTIFYICONDATA nid;
HHOOK mouseHook;
HHOOK keyboardHook;  // 新增键盘钩子
HWND hWnd;
HINSTANCE hInst;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);  // 新增键盘钩子处理

// 浮动窗口结构体
struct FloatingWindowInfo {
    HWND hWnd;
    HFONT hFont;
    COLORREF bgColor;
    COLORREF textColor;
    HBRUSH hBgBrush;
    HPEN hBorderPen;  // 新增边框画笔
};

FloatingWindowInfo g_floatingWindow = { 0 };

// ================== 函数声明 ==================
void CreateFloatingWindow();
void UpdateFloatingWindow(LPCTSTR text, POINT cursorPos);
void DestroyFloatingWindow();
LRESULT CALLBACK FloatingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// ================== 托盘图标相关 ==================
// 创建隐藏的主窗口（处理消息循环）
void CreateTrayIcon(LPCTSTR title, LPCTSTR iconPath) {
    hInst = GetModuleHandle(NULL);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = _T("TrayWindow");
    RegisterClassEx(&wc);

    // 关键：使用 WS_EX_TOOLWINDOW 和 WS_POPUP 样式
    hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,  // 不在任务栏显示
        _T("TrayWindow"),
        _T(""),
        WS_POPUP,          // 无边框
        0, 0, 0, 0,        // 窗口大小为0
        NULL,
        NULL,
        hInst,
        NULL
    );

    // 彻底隐藏窗口
    ShowWindow(hWnd, SW_HIDE);
    UpdateWindow(hWnd);

    // 添加托盘图标
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    _tcscpy_s(nid.szTip, title);
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// ================== 浮动窗口相关 ==================
void CreateFloatingWindow() {
    if (g_floatingWindow.hWnd == NULL) {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = FloatingWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = _T("FloatingWindowClass");
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassEx(&wc);

        // 创建无边框窗口（实际边框通过绘制实现）
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

        // 设置颜色：白底黑字
        g_floatingWindow.bgColor = RGB(255, 255, 255);  // 白色背景
        g_floatingWindow.textColor = RGB(0, 0, 0);      // 黑色文字

        // 创建GDI资源
        g_floatingWindow.hBgBrush = CreateSolidBrush(g_floatingWindow.bgColor);
        g_floatingWindow.hBorderPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));  // 黑色边框
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

    // 窗口尺寸 = 文本区域 + 边距 + 边框
    int width = textRect.right + 20 + 2;   // 左右边距20，边框左右各1
    int height = textRect.bottom + 10 + 2; // 上下边距10，边框上下各1

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
    if (g_floatingWindow.hBorderPen) DeleteObject(g_floatingWindow.hBorderPen);  // 释放边框画笔
    memset(&g_floatingWindow, 0, sizeof(g_floatingWindow));
}

// ================== 窗口绘制逻辑 ==================
LRESULT CALLBACK FloatingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 双缓冲绘制
        HDC hMemDC = CreateCompatibleDC(hdc);
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);

        // 绘制背景
        FillRect(hMemDC, &clientRect, g_floatingWindow.hBgBrush);

        // 绘制黑色边框
        HPEN hOldPen = (HPEN)SelectObject(hMemDC, g_floatingWindow.hBorderPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
        Rectangle(hMemDC, 0, 0, clientRect.right, clientRect.bottom);

        // 绘制文本
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, g_floatingWindow.hFont);
        SetBkMode(hMemDC, TRANSPARENT);
        SetTextColor(hMemDC, g_floatingWindow.textColor);

        TCHAR text[MAX_PATH];
        GetWindowText(hWnd, text, MAX_PATH);
        RECT textRect = clientRect;
        textRect.left += 11;  // 左边距10 + 边框1
        textRect.top += 6;    // 上边距5 + 边框1

        DrawText(hMemDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 复制到屏幕
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hMemDC, 0, 0, SRCCOPY);

        // 清理资源
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

// ================== 快捷键检测 ==================
void HandleMouseEvent() {
    // 检测 Ctrl + Alt 是否同时按下
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

// ================== 键盘钩子处理 ==================
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKb = (KBDLLHOOKSTRUCT*)lParam;
        // 当 Ctrl 或 Alt 键释放时立即隐藏窗口
        if (wParam == WM_KEYUP && (pKb->vkCode == VK_CONTROL || pKb->vkCode == VK_MENU)) {
            AnimateWindow(g_floatingWindow.hWnd, 100, AW_HIDE | AW_BLEND);
            ShowWindow(g_floatingWindow.hWnd, SW_HIDE);
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// ================== 主程序入口 ==================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CreateTrayIcon(_T("进程检测器 - Ctrl+Alt"), _T(""));

    // 同时安装鼠标和键盘钩子
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, hInstance, 0);
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理钩子和资源
    UnhookWindowsHookEx(mouseHook);
    UnhookWindowsHookEx(keyboardHook);
    DestroyFloatingWindow();
    Shell_NotifyIcon(NIM_DELETE, &nid);
    return 0;
}

// ================== 其他窗口过程 ==================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TRAYICON && LOWORD(lParam) == WM_RBUTTONUP) {
        PostQuitMessage(0);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        HandleMouseEvent();  // 所有鼠标事件均触发检测
    }
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}
//by deepseek
/**************************************************************************************************
 * System libs
 **************************************************************************************************/
#include <windows.h>

/**************************************************************************************************
 * Standard libs
 **************************************************************************************************/
#include <cstdio>
#include <vector>
#include <string>

#include "resource.h" // for version info

/**************************************************************************************************
 * Simple types and constants
 **************************************************************************************************/
typedef wchar_t                 Char;
typedef std::wstring            String;

const Char*	 JTM_WINDOW_NAME    = L"Just Tray Me";
const Char*	 DIR_SEP            = L"\\";
const Char*	 KILL_CMD_NAME      = L"KILL";

enum {
	WM_BASE                     = WM_USER + 100,
    WM_EXIT                     = WM_BASE + 1,
	WM_TOGGLE                   = WM_BASE + 2,
	WM_START                    = WM_BASE + 3,
	WM_STOP                     = WM_BASE + 4,

	TARGET_STATE_VISIBLE        = 1000,
	TARGET_STATE_HIDDEN         = 1001,
	TARGET_STATE_STOPPED        = 1002,

    UPDATE_INTERVAL             = 200,
};

/**************************************************************************************************
 * Utility functions
 **************************************************************************************************/
struct Util {
    static void msgt(const String& title, const String& format, ...) {
	    static Char msgBuf[4096] = { 0 };
	    va_list arglist;
	    va_start(arglist, format);
        vswprintf(msgBuf, format.c_str(), arglist);
	    va_end(arglist);
        ::MessageBox(0, msgBuf, title.c_str(), MB_OK | MB_ICONINFORMATION);
    }
    static void msg(const String& format, ...) {
	    static Char msgBuf[4096] = { 0 };
	    va_list arglist;
	    va_start(arglist, format);
        vswprintf(msgBuf, format.c_str(), arglist);
	    va_end(arglist);
        ::MessageBox(0, msgBuf, (String(L"JTM v") + JTM_VERSION_STR).c_str(), MB_OK | MB_ICONINFORMATION);
    }
    static bool exec(const String& cmd, const String& params) {
        return SUCCEEDED(::ShellExecute(0, L"open", cmd.c_str(), params.c_str(), 0, SW_SHOWNORMAL));
    }
    static bool killProcess(const DWORD procId) {
        bool killed = false;
        HANDLE proc = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId); 
		if (proc) {
            killed = SUCCEEDED(::TerminateProcess(proc, 0));
			::CloseHandle(proc);
		}
        return killed;
    }
    static bool killProcess(const String& windowName) {
        DWORD procId;
        if (Util::findProcess(windowName, procId)) {
            if (Util::killProcess(procId)) return true;
            Util::msg(L"Failed to kill process %d", procId);
        } else {
            Util::msg(L"Target window '%s' not found", windowName.c_str());
        }
        return false;
    }
    static bool findProcess(const String& windowName, DWORD& procId, HWND& hWnd) {
        hWnd = 0;
        procId = 0;
        Char title[1024] = { 0 };
	    while (hWnd = FindWindowEx(0, hWnd, 0, 0)) {
            ::GetWindowText(hWnd, title, sizeof(title));
            if (wcsstr(title, windowName.c_str())) {
                ::GetWindowThreadProcessId(hWnd, &procId);
                return true;
            }
	    }
        return false;
    }
    static bool findProcess(const String& windowName, DWORD& procId) {
        HWND hWnd;
        return findProcess(windowName, procId, hWnd);
    }
    static bool findWindow(const String& windowName, HWND& hWnd) {
        DWORD procId;
        return findProcess(windowName, procId, hWnd);
    }
    static void toggleWindow(HWND hWnd) {
        showWindow(hWnd, isWindowVisible(hWnd));
    }
    static bool isWindowVisible(HWND hWnd) {
        return (::GetWindowLong(hWnd, GWL_STYLE) & WS_VISIBLE) == WS_VISIBLE;
    }
    static void showWindow(HWND hWnd, bool hide) {
        if (hide) {
            ::ShowWindow(hWnd, SW_HIDE);
        } else {
            ::ShowWindow(hWnd, SW_SHOWNORMAL);
            ::SetForegroundWindow(hWnd);
        }
    }
    static bool fileExists(const String& path) {
        DWORD dwAttrib = ::GetFileAttributes(path.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }
};

/**************************************************************************************************
 * App configuration
 **************************************************************************************************/
struct Cfg {
    String  monitorName;
    String  targetWindow;
    String  cmdStart;
    String  cmdStartParams;
    String  cmdStop;
    String  cmdStopParams;
    String  iconOn;
    String  iconOff;
    bool    autostart;

    bool init(const String& cfgPath) {
        _cfgPath = cfgPath;
        if (!Util::fileExists(cfgPath)) {
            Util::msg(L"Config file not found: '%s'", cfgPath.c_str());
            return false;
        }
        if (!readOption(L"Window", false, targetWindow))        return false;
        if (!readOption(L"Name", false, monitorName))           return false;
        if (!readOption(L"StartCmd", false, cmdStart))          return false;
        if (!readOption(L"StartParams", true, cmdStartParams))  return false;
        if (!readOption(L"StopCmd", false, cmdStop))            return false;
        if (!readOption(L"StopParams", true, cmdStopParams))    return false;
        if (!readOption(L"IconOn", true, iconOn))               return false;
        if (!readOption(L"IconOff", true, iconOff))             return false;
        if (!readOption(L"StopParams", true, cmdStopParams))    return false;
        if (!readOption(L"Autostart", true, autostart))         return false;
        return true;
    }

private:
    String  _cfgPath;

    bool readOption(const String& optionName, bool allowEmpty, String& option) {
        Char buf[1024] = { 0 };
        ::GetPrivateProfileString(L"Target", optionName.c_str(), L"", buf, sizeof(buf), _cfgPath.c_str());
        if (wcslen(buf) < 1 && !allowEmpty) {
            Util::msg(L"Config parameter 'Target -> %s' not defined", optionName.c_str());
            return false;
        }
        option = buf;
        return true;
    }
    bool readOption(const String& optionName, bool allowEmpty, bool& option) {
        option = false;
        String tmp;
        if (!readOption(optionName, allowEmpty, tmp) && !allowEmpty) return false;
        option = tmp == L"yes" || tmp == L"true";
        return true;
    }
};

/**************************************************************************************************
 * The app
 **************************************************************************************************/
struct App {
    App() : _window(0), _targetState(0), _menu(0) {
    }
    bool init(const Cfg& cfg) {
        _cfg = cfg;
        if (!initIcons()) return false;
        if (!initWindow()) return false;
        initTrayIcon();
        autostart();
        ::SetTimer(_window, 666, UPDATE_INTERVAL, 0);
        return true;
    }
    void deinit() {
        HWND hWnd;
        if (Util::findWindow(_cfg.targetWindow, hWnd)) {
            Util::showWindow(hWnd, false);
        }
        ::Shell_NotifyIcon(NIM_DELETE, &_notifyData);
	    ::PostQuitMessage(0);
    }
    void run() {
        for (MSG msg; ::GetMessage(&msg, 0, 0, 0); ) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

private:
    HWND            _window;
    HMENU           _menu;
    NOTIFYICONDATA	_notifyData;
    HICON           _hIcon, _hIconOn, _hIconOff;

    Cfg             _cfg;
    int             _targetState;

    bool initIcons() {
        _hIcon = ::LoadIcon(::GetModuleHandle(0), L"JTM_ICON_NAME");
        _hIconOn = loadIcon(_cfg.iconOn, L"JTM_ICON_ON");
        _hIconOff = loadIcon(_cfg.iconOff, L"JTM_ICON_OFF");
        if (!_hIconOn)  Util::msg(L"Failed to load IconOn");
        if (!_hIconOff) Util::msg(L"Failed to load IconOff");
        return _hIcon && _hIconOn && _hIconOff;
    }
    HICON loadIcon(const String& fileName, const String& defaultName) {
        return fileName.length() < 1 
            ? ::LoadIcon(::GetModuleHandle(0), defaultName.c_str())
            : (HICON)::LoadImage(0, fileName.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    }
    bool initWindow() {
	    WNDCLASS wc = { 0 };
	    wc.style            = CS_HREDRAW | CS_VREDRAW;
	    wc.lpfnWndProc      = windowProc;
	    wc.hInstance        = ::GetModuleHandle(0);
        wc.hIcon            = _hIcon;
	    wc.lpszClassName    = JTM_WINDOW_NAME;
	    if (!::RegisterClass(&wc)) return false;
	    _window = ::CreateWindow(JTM_WINDOW_NAME, JTM_WINDOW_NAME, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, 0, 0, ::GetModuleHandle(0), 0);
        if (!_window) return false;
        ::SetWindowLongPtr(_window, GWLP_USERDATA, (LONG_PTR)(App*)this);
        return true;
    }
    bool initTrayIcon() {
        memset(&_notifyData, 0, sizeof(NOTIFYICONDATA));
        wcscpy(_notifyData.szTip, (L"Just Tray Me: " + _cfg.monitorName).c_str());
        _notifyData.cbSize              = sizeof(NOTIFYICONDATA);
        _notifyData.hWnd                = _window;
        _notifyData.uID                 = JTM_ICON_ID;
        _notifyData.uFlags              = NIF_MESSAGE + NIF_ICON + NIF_TIP;
        _notifyData.hIcon               = _hIcon;
        _notifyData.uCallbackMessage    = WM_BASE;
        ::Shell_NotifyIcon(NIM_ADD, &_notifyData);
        return true;
    }
    void autostart() {
        HWND hWnd;
        if (_cfg.autostart && !Util::findWindow(_cfg.targetWindow, hWnd)) startTarget();
    }
    void showMenu() {
        RECT rWorkArea;
        POINT pos;
        ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rWorkArea, 0);
        ::GetCursorPos(&pos);

        LONG pos_x = pos.x, pos_y = pos.y;
        if (pos_x < rWorkArea.left)         pos_x = rWorkArea.left - 1;
        else if (pos_x > rWorkArea.right)   pos_x = rWorkArea.right - 1;
        if (pos_y < rWorkArea.top)          pos_y = rWorkArea.top - 1;
        else if (pos_y > rWorkArea.bottom)  pos_y = rWorkArea.bottom - 1;

	    ::SetForegroundWindow(_window);
	    ::TrackPopupMenuEx(_menu, ::GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_LEFTBUTTON, pos_x, pos_y, _window, 0);
    }
    void toggleTarget() {
        HWND hWnd;
        if (Util::findWindow(_cfg.targetWindow, hWnd)) Util::toggleWindow(hWnd);
    }
    void startTarget() {
        Util::exec(_cfg.cmdStart, _cfg.cmdStartParams);
    }
    void stopTarget() {
        if (_wcsicmp(_cfg.cmdStop.c_str(), KILL_CMD_NAME) != 0) {
            Util::exec(_cfg.cmdStop, _cfg.cmdStopParams);
        } else {
            Util::killProcess(_cfg.targetWindow);
        }
    }
    void updateIcon() {
        _notifyData.hIcon = _targetState == TARGET_STATE_STOPPED ? _hIconOff : _hIconOn;
        ::Shell_NotifyIcon(NIM_MODIFY, &_notifyData);
    }
    bool updateMenu() {
        bool isRunning = _targetState != TARGET_STATE_STOPPED;
        ::DestroyMenu(_menu);
        _menu = ::CreatePopupMenu();
        addMenuItem(WM_TOGGLE, (L"Toggle " + _cfg.monitorName).c_str(), isRunning);
        addMenuSeparator();
        addMenuItem(WM_START, (L"Start " + _cfg.monitorName).c_str(), !isRunning);
        addMenuItem(WM_STOP, (L"Stop " + _cfg.monitorName).c_str(), isRunning);
        addMenuSeparator();
        addMenuItem(WM_EXIT, L"Exit");
        ::SetMenuDefaultItem(_menu, 0, true);
        return true;
    }
    bool addMenuSeparator() {
        return SUCCEEDED(::AppendMenu(_menu, MF_SEPARATOR, 0, 0));
    }
    bool addMenuItem(const int msg, const String& caption, const bool enabled = true) {
        return SUCCEEDED(::AppendMenu(_menu, enabled ? MF_STRING : MF_GRAYED, msg, caption.c_str()));
    }
    void update() {
        int lastState = _targetState;
        HWND hWnd;
        if (!Util::findWindow(_cfg.targetWindow, hWnd)) {
            _targetState = TARGET_STATE_STOPPED;
        } else {
            _targetState = Util::isWindowVisible(hWnd) ? TARGET_STATE_VISIBLE : TARGET_STATE_HIDDEN;
        }
        if (lastState != _targetState) {
            updateIcon();
            updateMenu();
        }
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        App* app = (App*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (msg == WM_COMMAND) {
			switch (LOWORD(wparam)) {
				case WM_EXIT:           app->deinit(); break;
				case WM_TOGGLE:         app->toggleTarget(); break;
				case WM_START:          app->startTarget(); break;
				case WM_STOP:           app->stopTarget(); break;
            }
        } else if (msg == WM_BASE) {
			switch (LOWORD(lparam)) {
				case WM_RBUTTONUP:      app->showMenu(); break;
				case WM_LBUTTONDBLCLK:  app->toggleTarget(); break;
			}
        } else if (msg == WM_TIMER) {
            app->update();
        }
	    return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
};

/**************************************************************************************************
 * App entry point
 **************************************************************************************************/
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPTSTR cmdLine, int) {
    Cfg cfg;
    if (!cfg.init(cmdLine)) {
        return 0;
    }
    App app;
    if (!app.init(cfg)) {
        Util::msg(L"Just Tray Me! did not start :(\n\nPlease review the config file:  %s", cmdLine);
        return 0;
    }
    app.run();
	return 0;
}

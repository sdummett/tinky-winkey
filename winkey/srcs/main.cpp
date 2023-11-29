// winkey.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>

// This variable will hold hook instance
HHOOK hHook = NULL;

// I define the keyboard hook 
LRESULT CALLBACK LowLevelKeyboardProc(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
) {
    if (nCode < 0) {
        std::cout << "Wrong hook, need to pass to the next hook...\n";
        return CallNextHookEx(hHook, nCode, wParam, lParam);
    }

    KBDLLHOOKSTRUCT* pKbStruct = (KBDLLHOOKSTRUCT*)lParam;
    char keyName[1024] = { 0 };
    std::cout << "sizeof keyName: " << sizeof(keyName) << std::endl;

    if (wParam == WM_KEYDOWN) {
        std::cout << "WM_KEYDOWN\n";
        std::cout << "Key down - Virtual Key Code: " << pKbStruct->vkCode << std::endl;
        std::cout << std::hex << "Key down - Virtual Key Code: 0x" << pKbStruct->vkCode << std::dec << std::endl;

        memset(keyName, 0, sizeof(keyName));
        DWORD dwMsg = 1;
        dwMsg += pKbStruct->scanCode << 16;
        dwMsg += pKbStruct->flags << 24;
        if (GetKeyNameTextA(dwMsg, keyName, sizeof(keyName) - 1)) {
            std::cout << "Key name: " << keyName << std::endl;
        }
        else {
            std::cout << "Last error: " << GetLastError() << std::endl;
        }
    }

    if (wParam == WM_SYSKEYDOWN) {
        std::cout << "WM_SYSKEYDOWN\n";
        std::cout << "Key down - Virtual Key Code: " << pKbStruct->vkCode << std::endl;
        std::cout << std::hex << "Key down - Virtual Key Code: 0x" << pKbStruct->vkCode << std::dec << std::endl;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main()
{
    std::cout << "Winkey !\n";

    // Get the handle to the current module (executable)
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Install the hook low-level keyboard hook
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

    if (hHook == NULL) {
        std::cout << "Failed to install keyboard hook.\n";
        return 1;
    }

    std::cout << "Keyboard hook installed.\n";

    // Message loop to keep the program running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook the keyboard hook before exiting
    UnhookWindowsHookEx(hHook);

    std::cout << "Leaving winkey =)\n";
    return 0;
}

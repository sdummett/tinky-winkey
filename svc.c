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

#define WINKEY_PATH "C:\\Users\\rever\\source\\repos\\sdummett-at-42\\tinky-winkey\\winkey.exe"
#define SVC_NAME "tinky"
#define SVC_ERROR 1

SERVICE_STATUS          g_svc_status;
SERVICE_STATUS_HANDLE   g_svc_status_handle;
HANDLE                  g_svc_stop_event = NULL;
HANDLE                  g_process = NULL; // Handle global pour le processus

static VOID WINAPI svc_ctrl_handler(DWORD);
static VOID WINAPI svc_main(DWORD, LPTSTR*);

static VOID report_svc_status(DWORD, DWORD, DWORD);
static VOID svc_init(DWORD, LPTSTR*);
static VOID svc_report_event(LPTSTR);
static void print_error(const char* msg);

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
static VOID WINAPI svc_main(DWORD argc, LPTSTR* argv)
{
    // Register the handler function for the service

    g_svc_status_handle = RegisterServiceCtrlHandler(
        SVC_NAME,
        svc_ctrl_handler);

    if (!g_svc_status_handle)
    {
		print_error("Failed to register service control handler");
        svc_report_event(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // These SERVICE_STATUS members remain as set here

    g_svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svc_status.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    report_svc_status(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    svc_init(argc, argv);
}

static void start_process(LPCTSTR lpApplicationName)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Launch the process
    if (!CreateProcess(lpApplicationName,   // Name of the executable file
        NULL,                // Command line parameters
        NULL,                // Process security attributes
        NULL,                // Thread security attributes
        FALSE,               // Inherit handles
        0,                   // Creation flags
        NULL,                // Environment block
        NULL,                // Current directory
        &si,                 // Startup information
        &pi))                // Process information
    {
        print_error("Failed to create process");
        return;
    }

    g_process = pi.hProcess; // Store the process handle globally

    // Close the handle for the thread
    CloseHandle(pi.hThread);
}

static void stop_process(void)
{
    if (g_process != NULL)
    {
        TerminateProcess(g_process, 0);
        CloseHandle(g_process);
        g_process = NULL;
    }
}

static BOOL get_winlogon_pid(DWORD* pProcessId)
{
    // Take a snapshot of all processes in the system.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    // Iterate through the list of processes to find winlogon.exe.
    if (Process32First(snapshot, &pe))
    {
        do
        {
            // Compare the process name to "winlogon.exe" (case insensitive).
            if (_tcsicmp(pe.szExeFile, _T("winlogon.exe")) == 0)
            {
                // If found, set the process ID and return TRUE.
                *pProcessId = pe.th32ProcessID;
                CloseHandle(snapshot);
                return TRUE;
            }
        } while (Process32Next(snapshot, &pe));
    }

    // Clean up the snapshot handle and return FALSE if the process was not found.
    CloseHandle(snapshot);
    return FALSE;
}

static BOOL impersonate_winlogon(void)
{
    DWORD winlogon_pid = 0;

    // Get the process ID of winlogon.exe.
    if (!get_winlogon_pid(&winlogon_pid))
        return FALSE;

    // Open the winlogon process with PROCESS_QUERY_INFORMATION access.
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, winlogon_pid);
    if (process == NULL) {
		print_error("Failed to open winlogon process");
        return FALSE;
    }

    // Open the access token associated with the winlogon process.
    HANDLE token;
    if (!OpenProcessToken(process, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &token))
    {
		print_error("Failed to open process token");
        CloseHandle(process);
        return FALSE;
    }

    // Duplicate the token to create a primary token with the necessary privileges.
    HANDLE dup_token;
    if (!DuplicateTokenEx(token, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &dup_token))
    {
		print_error("Failed to duplicate token");
        CloseHandle(token);
        CloseHandle(process);
        return FALSE;
    }

    // Impersonate the logged-on user using the duplicated token.
    if (!ImpersonateLoggedOnUser(dup_token))
    {
        // Clean up the handles if impersonation fails.
		print_error("Failed to impersonate logged-on user");
        CloseHandle(dup_token);
        CloseHandle(token);
        CloseHandle(process);
        return FALSE;
    }

    // Close the handles after successful impersonation.
    CloseHandle(dup_token);
    CloseHandle(token);
    CloseHandle(process);
    return TRUE;
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
static VOID svc_init(DWORD argc, LPTSTR* argv)
{
	(void)argc, (void)argv;
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call report_svc_status() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   report_svc_status with SERVICE_STOPPED.

    // Create an event. The control handler function, svc_ctrl_handler,
    // signals this event when it receives the stop control code.

    // Attempt to impersonate the winlogon.exe process.
    if (!impersonate_winlogon())
    {
        report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    g_svc_stop_event = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (g_svc_stop_event == NULL)
    {
        report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report running status when initialization is complete.
    report_svc_status(SERVICE_RUNNING, NO_ERROR, 0);

    // Perform work until service stops.
	start_process(WINKEY_PATH);

    WaitForSingleObject(g_svc_stop_event, INFINITE);
    report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
static VOID report_svc_status(DWORD current_state,
    DWORD win32_exit_code,
    DWORD wait_hint)
{
    static DWORD checkpoint = 1;

    // Fill in the SERVICE_STATUS structure.

    g_svc_status.dwCurrentState = current_state;
    g_svc_status.dwWin32ExitCode = win32_exit_code;
    g_svc_status.dwWaitHint = wait_hint;

    if (current_state == SERVICE_START_PENDING)
        g_svc_status.dwControlsAccepted = 0;
    else g_svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((current_state == SERVICE_RUNNING) ||
        (current_state == SERVICE_STOPPED))
        g_svc_status.dwCheckPoint = 0;
    else g_svc_status.dwCheckPoint = checkpoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(g_svc_status_handle, &g_svc_status);
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
static VOID WINAPI svc_ctrl_handler(DWORD ctrl)
{
    // Handle the requested control code. 

    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
        report_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);

		// Stop the process
		stop_process();

        // Signal the service to stop.

        SetEvent(g_svc_stop_event);
        report_svc_status(g_svc_status.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
static VOID svc_report_event(LPTSTR function)
{
    HANDLE event_source;
    LPCTSTR strings[2];
    TCHAR buffer[80];

    event_source = RegisterEventSource(NULL, SVC_NAME);

    if (event_source != NULL)
    {
        StringCchPrintf(buffer, 80, TEXT("%s failed with %d"), function, GetLastError());

        strings[0] = SVC_NAME;
        strings[1] = buffer;

        ReportEvent(event_source, // event log handle
            EVENTLOG_ERROR_TYPE,  // event type
            0,                    // event category
            SVC_ERROR,            // event identifier
            NULL,                 // no security identifier
            2,                    // size of strings array
            0,                    // no binary data
            strings,              // array of strings
            NULL);                // no binary data

        DeregisterEventSource(event_source);
    }
}

// Fonction pour obtenir le message d'erreur correspondant au code d'erreur
static void print_error(const char *msg) {
    DWORD err = GetLastError();
    LPSTR error_text = NULL;

    // Obtenir le message d'erreur associé au code d'erreur
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&error_text,
        0,
        NULL
    );

    // Afficher le message d'erreur
    fprintf(stderr, "%s\n", msg);
    if (error_text) {
        fprintf(stderr, "Reason: (%lu) %s", err, error_text);
        // Libérer la mémoire allouée pour le message d'erreur
        LocalFree(error_text);
    }
}

static void install_service(void) {
    TCHAR unquoted_path[MAX_PATH];
    if (!GetModuleFileName(NULL, unquoted_path, MAX_PATH))
    {
		print_error("Failed to get module file name");
        return;
    }

    // In case the path contains a space, it must be quoted so that
    // it is correctly interpreted. For example,
    // "d:\my share\myservice.exe" should be specified as
    // ""d:\my share\myservice.exe"".
    TCHAR path[MAX_PATH];
    StringCbPrintf(path, MAX_PATH, TEXT("\"%s\""), unquoted_path);

    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

    SC_HANDLE service = CreateService(
        scm,                          // Handle to the service control manager (SCM)
        SVC_NAME,                     // Service name (short name used by the system)
        SVC_NAME,                     // Display name (name shown in the services console)
        SERVICE_ALL_ACCESS,           // Full access rights
        SERVICE_WIN32_OWN_PROCESS,    // Service type: runs in its own process
        SERVICE_DEMAND_START,         // Start type: manual start on demand
        SERVICE_ERROR_NORMAL,         // Error control: log error and continue startup
        path,                         // Path to the service executable
        NULL,                         // Load order group (NULL if not part of a group)
        NULL,                         // Tag ID (NULL if no unique tag is needed)
        NULL,                         // Dependencies (NULL if no dependencies)
        NULL,                         // Service start name (NULL for LocalSystem account)
        NULL                          // Password (NULL if not required)
    );

    if (service == NULL) {
        print_error("Failed to create service");
    }
    else {
        printf("Service installed successfully\n");
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
}

static void wait_status_stopped(SC_HANDLE service, SC_HANDLE scm) {
    // Check the status in case the service is not stopped. 

    SERVICE_STATUS_PROCESS status;
    DWORD old_checkpoint;
    DWORD start_tick_count;
    DWORD wait_time;
    DWORD bytes_needed;

    if (!QueryServiceStatusEx(
        service,                        // handle to service 
        SC_STATUS_PROCESS_INFO,         // information level
        (LPBYTE)&status,                // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &bytes_needed))                 // size needed if buffer is too small
    {
		print_error("Failed to query service status");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    // Check if the service is already running. It would be possible 
    // to stop the service here, but for simplicity this example just returns. 

    if (status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING)
    {
        printf("Cannot start the service because it is already running\n");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    // Save the tick count and initial checkpoint.

    start_tick_count = GetTickCount();
    old_checkpoint = status.dwCheckPoint;

    // Wait for the service to stop before attempting to start it.

    while (status.dwCurrentState == SERVICE_STOP_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        wait_time = status.dwWaitHint / 10;

        if (wait_time < 1000)
            wait_time = 1000;
        else if (wait_time > 10000)
            wait_time = 10000;

        Sleep(wait_time);

        // Check the status until the service is no longer stop pending. 

        if (!QueryServiceStatusEx(
            service,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // information level
            (LPBYTE)&status,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &bytes_needed))              // size needed if buffer is too small
        {
			print_error("Failed to query service status");
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return;
        }

        if (status.dwCheckPoint > old_checkpoint)
        {
            // Continue to wait and check.

            start_tick_count = GetTickCount();
            old_checkpoint = status.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - start_tick_count > status.dwWaitHint)
            {
                printf("Timeout waiting for service to stop\n");
                CloseServiceHandle(service);
                CloseServiceHandle(scm);
                return;
            }
        }
    }
}

static void wait_status_pending(SC_HANDLE service, SC_HANDLE scm) {
    // Check the status until the service is no longer start pending. 

    SERVICE_STATUS_PROCESS status;
    DWORD old_checkpoint;
    DWORD start_tick_count;
    DWORD wait_time;
    DWORD bytes_needed;

    if (!QueryServiceStatusEx(
        service,                        // handle to service 
        SC_STATUS_PROCESS_INFO,         // info level
        (LPBYTE)&status,                // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &bytes_needed))                 // if buffer too small
    {
		print_error("Failed to query service status");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    // Save the tick count and initial checkpoint.

    start_tick_count = GetTickCount();
    old_checkpoint = status.dwCheckPoint;

    while (status.dwCurrentState == SERVICE_START_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth the wait hint, but no less than 1 second and no 
        // more than 10 seconds. 

        wait_time = status.dwWaitHint / 10;

        if (wait_time < 1000)
            wait_time = 1000;
        else if (wait_time > 10000)
            wait_time = 10000;

        Sleep(wait_time);

        // Check the status again. 

        if (!QueryServiceStatusEx(
            service,             // handle to service 
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE)&status,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &bytes_needed))              // if buffer too small
        {
			print_error("Failed to query service status");
            break;
        }

        if (status.dwCheckPoint > old_checkpoint)
        {
            // Continue to wait and check.

            start_tick_count = GetTickCount();
            old_checkpoint = status.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - start_tick_count > status.dwWaitHint)
            {
                // No progress made within the wait hint.
                break;
            }
        }
    }

    // Determine whether the service is running.

    if (status.dwCurrentState == SERVICE_RUNNING)
    {
        printf("Service started successfully.\n");
    }
    else
    {
        printf("Service not started. \n");
        printf("  Current State: %ld\n", status.dwCurrentState);
        printf("  Exit Code: %ld\n", status.dwWin32ExitCode);
        printf("  Check Point: %ld\n", status.dwCheckPoint);
        printf("  Wait Hint: %ld\n", status.dwWaitHint);
    }
}

static void start_service(void) {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

	wait_status_stopped(service, scm);

    if (!StartService(service, 0, NULL)) {
        print_error("Failed to start service");
    }
    else {
        printf("Service start pending...\n");
    }

	wait_status_pending(service, scm);

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

static void stop_service(void) {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_STOP);
    if (service == NULL) {
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

    SERVICE_STATUS status;
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        print_error("Failed to stop service");
    }
    else {
        printf("Service stopped successfully\n");
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

static BOOL is_service_running(SC_HANDLE service)
{
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytes_needed;

    if (!QueryServiceStatusEx(
        service,                          // Handle to service
        SC_STATUS_PROCESS_INFO,           // Information level
        (LPBYTE)&ssp,                     // Buffer
        sizeof(SERVICE_STATUS_PROCESS),   // Buffer size
        &bytes_needed))                   // Bytes needed
    {
		print_error("Failed to query the service status");
        return FALSE;
    }

    // Check if the service is running
    return (ssp.dwCurrentState == SERVICE_RUNNING);
}

static void delete_service(void) {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

	if (is_service_running(service)) {
		stop_service();
	}

    if (!DeleteService(service)) {
        print_error("Failed to delete service");
    }
    else {
        printf("Service deleted successfully\n");
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        SERVICE_TABLE_ENTRY dispatch_table[] =
        {
            { SVC_NAME, (LPSERVICE_MAIN_FUNCTION)svc_main },
            { NULL, NULL }
        };

        // This call returns when the service has stopped. 
        // The process should simply terminate when the call returns.

        if (!StartServiceCtrlDispatcher(dispatch_table))
        {
			print_error("Failed StartServiceCtrlDispatcher");
            svc_report_event(TEXT("StartServiceCtrlDispatcher"));
        }
        return 0;
    }

    if (strcmp(argv[1], "install") == 0) {
        install_service();
    }
    else if (strcmp(argv[1], "start") == 0) {
        start_service();
    }
    else if (strcmp(argv[1], "stop") == 0) {
        stop_service();
    }
    else if (strcmp(argv[1], "delete") == 0) {
        delete_service();
    }
    else {
        printf("Invalid option. Usage: svc <install|start|stop|delete>\n");
        return 1;
    }

    return 0;
}
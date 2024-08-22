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

// Path to the executable file that the service will manage.
#define WINKEY_PATH "C:\\Users\\rever\\source\\repos\\sdummett-at-42\\tinky-winkey\\winkey.exe"

// Name of the service used for registration and control.
#define SVC_NAME "tinky"

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
static VOID WINAPI svc_main(DWORD, LPTSTR*);

static VOID report_svc_status(DWORD, DWORD, DWORD);
static VOID svc_init(DWORD, LPTSTR*);
static VOID svc_report_event(LPTSTR);
static void print_error(const char* msg);

static VOID WINAPI svc_main(DWORD argc, LPTSTR* argv)
{
    // Register the handler function for the service.
    // This function connects the service to the SCM (Service Control Manager).
    // 'svc_ctrl_handler' is the function that will handle control requests (e.g., stop, pause).
    g_svc_status_handle = RegisterServiceCtrlHandler(
        SVC_NAME,
        svc_ctrl_handler);

    // Check if the registration of the control handler failed.
    // If it fails, report an error event and exit the function.
    if (!g_svc_status_handle)
    {
        print_error("Failed to register service control handler");
        svc_report_event(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // Initialize the SERVICE_STATUS structure members that will remain constant.
    // 'dwServiceType' indicates the type of service (here, it's a service running in its own process).
    // 'dwServiceSpecificExitCode' is set to 0, meaning no specific exit code is used.
    g_svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svc_status.dwServiceSpecificExitCode = 0;

    // Report the initial status of the service to the SCM.
    // This informs the SCM that the service is in the process of starting (SERVICE_START_PENDING).
    // A delay of 3000 milliseconds is provided for the start-up process.
    report_svc_status(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and start the service's main work.
    // This function will include the core logic required for the service to operate.
    svc_init(argc, argv);
}

static void start_process(LPCTSTR lpApplicationName)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    // Initialize the STARTUPINFO structure with zeros and set its size.
    // This structure is used to specify window settings and other options for the process being created.
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create the new process.
    // 'CreateProcess' launches the executable specified by 'lpApplicationName'.
    // It provides information about how the process should be started and managed.
    if (!CreateProcess(lpApplicationName,   // Name of the executable file
        NULL,                // Command line parameters (NULL for default)
        NULL,                // Process security attributes (NULL for default)
        NULL,                // Thread security attributes (NULL for default)
        FALSE,               // Do not inherit handles
        0,                   // Default creation flags
        NULL,                // Use the current environment (NULL for default)
        NULL,                // Use the current directory (NULL for default)
        &si,                 // Pointer to STARTUPINFO structure
        &pi))                // Pointer to PROCESS_INFORMATION structure
    {
        // If process creation fails, report the error.
        print_error("Failed to create process");
        return;
    }

    // Store the handle of the newly created process globally for future use.
    // This handle can be used to manage or terminate the process.
    g_process = pi.hProcess;

    // Close the handle to the thread. This handle is no longer needed after process creation.
    CloseHandle(pi.hThread);
}

static void stop_process(void)
{
    // Check if there is a valid process handle.
    // 'g_process' should be non-NULL if a process is currently running.
    if (g_process != NULL)
    {
        // Terminate the process using its handle.
        // 'TerminateProcess' forcefully stops the process and can be used to stop the process immediately.
        // The exit code '0' is passed, which is a conventional value indicating normal termination.
        TerminateProcess(g_process, 0);

        // Close the handle to the process.
        // After termination, the handle is no longer needed and should be closed to avoid resource leaks.
        CloseHandle(g_process);

        // Set the global process handle to NULL.
        // This indicates that there is no longer an active process handle.
        g_process = NULL;
    }
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

static BOOL impersonate_winlogon(void)
{
    DWORD winlogon_pid = 0;

    // Retrieve the process ID of winlogon.exe.
    // 'get_winlogon_pid' is used to find the process ID.
    if (!get_winlogon_pid(&winlogon_pid))
        return FALSE;

    // Open the winlogon process with PROCESS_QUERY_INFORMATION access.
    // 'OpenProcess' is used to obtain a handle to the winlogon process.
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, winlogon_pid);
    if (process == NULL) {
        print_error("Failed to open winlogon process");
        return FALSE;
    }

    // Open the access token associated with the winlogon process.
    // 'OpenProcessToken' is used to get the token for the process.
    HANDLE token;
    if (!OpenProcessToken(process, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &token))
    {
        print_error("Failed to open process token");
        CloseHandle(process);
        return FALSE;
    }

    // Duplicate the token to create a primary token with necessary privileges.
    // 'DuplicateTokenEx' creates a new token with the desired access level.
    HANDLE dup_token;
    if (!DuplicateTokenEx(token, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &dup_token))
    {
        print_error("Failed to duplicate token");
        CloseHandle(token);
        CloseHandle(process);
        return FALSE;
    }

    // Impersonate the logged-on user using the duplicated token.
    // 'ImpersonateLoggedOnUser' sets the security context to the user represented by the token.
    if (!ImpersonateLoggedOnUser(dup_token))
    {
        // Clean up handles if impersonation fails.
        print_error("Failed to impersonate logged-on user");
        CloseHandle(dup_token);
        CloseHandle(token);
        CloseHandle(process);
        return FALSE;
    }

    // Close the handles after successful impersonation.
    // These handles are no longer needed after impersonation.
    CloseHandle(dup_token);
    CloseHandle(token);
    CloseHandle(process);
    return TRUE;
}

static VOID svc_init(DWORD argc, LPTSTR* argv)
{
    (void)argc, (void)argv;
    // TODO: Declare and initialize any necessary variables.
    // Periodically call 'report_svc_status()' with SERVICE_START_PENDING during initialization.
    // If initialization fails, call 'report_svc_status()' with SERVICE_STOPPED.

    // Attempt to impersonate the winlogon.exe process.
    // 'impersonate_winlogon()' is used to gain the security context of the winlogon process.
    if (!impersonate_winlogon())
    {
        // Report service status as stopped if impersonation fails.
        report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

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

    // Start the process with the specified executable path.
    // 'start_process()' is used to launch the process.
    start_process(WINKEY_PATH);

    // Wait indefinitely until the stop event is signaled.
    // The service will continue to run until the stop event is set.
    WaitForSingleObject(g_svc_stop_event, INFINITE);

    // Report service status as stopped when the stop event is signaled.
    // This indicates that the service has stopped successfully.
    report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
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

static VOID WINAPI svc_ctrl_handler(DWORD ctrl)
{
    // Handle the requested control code sent by the Service Control Manager.

    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
        // Report that the service is in the process of stopping.
        report_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Stop the running process associated with the service.
        stop_process();

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

static VOID svc_report_event(LPTSTR function)
{
    HANDLE event_source;
    LPCTSTR strings[2];
    TCHAR buffer[80];

    // Register the service as an event source to write to the event log.
    event_source = RegisterEventSource(NULL, SVC_NAME);

    if (event_source != NULL)
    {
        // Format the error message with the function name and error code.
        StringCchPrintf(buffer, 80, TEXT("%s failed with %d"), function, GetLastError());

        // Prepare the array of strings to log.
        strings[0] = SVC_NAME;   // Service name
        strings[1] = buffer;     // Error message

        // Report the error event to the event log.
        ReportEvent(event_source,  // Event log handle
            EVENTLOG_ERROR_TYPE,    // Event type (error)
            0,                     // Event category (not used)
            SVC_ERROR,             // Event identifier (custom identifier)
            NULL,                  // No security identifier
            2,                     // Number of strings in the array
            0,                     // No binary data
            strings,               // Array of strings to log
            NULL);                 // No binary data

        // Unregister the event source to clean up.
        DeregisterEventSource(event_source);
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

static void install_service(void)
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

    // Open a handle to the Service Control Manager with the ability to create services.
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

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

static void wait_status_stopped(SC_HANDLE service, SC_HANDLE scm)
{
    // Structure to hold the service status information.
    SERVICE_STATUS_PROCESS status;
    DWORD old_checkpoint;
    DWORD start_tick_count;
    DWORD wait_time;
    DWORD bytes_needed;

    // Query the current status of the service.
    // This retrieves information about the service's current state.
    if (!QueryServiceStatusEx(
        service,                        // Handle to the service
        SC_STATUS_PROCESS_INFO,         // Information level to query
        (LPBYTE)&status,                // Pointer to the SERVICE_STATUS_PROCESS structure
        sizeof(SERVICE_STATUS_PROCESS), // Size of the structure
        &bytes_needed))                 // Bytes needed if the buffer is too small
    {
        print_error("Failed to query service status");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    // If the service is already running or pending to stop, return.
    // We do not attempt to start the service if it is not in a stoppable state.
    if (status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING)
    {
        printf("Cannot start the service because it is already running\n");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    // Record the starting tick count and checkpoint value.
    start_tick_count = GetTickCount();
    old_checkpoint = status.dwCheckPoint;

    // Wait for the service to stop. Loop until the service state is no longer "STOP_PENDING".
    while (status.dwCurrentState == SERVICE_STOP_PENDING)
    {
        // Calculate wait time. Wait time is a fraction of the wait hint but constrained between 1 and 10 seconds.
        wait_time = status.dwWaitHint / 10;

        if (wait_time < 1000)
            wait_time = 1000;
        else if (wait_time > 10000)
            wait_time = 10000;

        Sleep(wait_time);

        // Check the status of the service again.
        if (!QueryServiceStatusEx(
            service,                     // Handle to the service
            SC_STATUS_PROCESS_INFO,     // Information level to query
            (LPBYTE)&status,             // Pointer to the SERVICE_STATUS_PROCESS structure
            sizeof(SERVICE_STATUS_PROCESS), // Size of the structure
            &bytes_needed))              // Bytes needed if the buffer is too small
        {
            print_error("Failed to query service status");
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return;
        }

        // If the checkpoint has increased, reset the start tick count.
        // This indicates that progress is being made.
        if (status.dwCheckPoint > old_checkpoint)
        {
            start_tick_count = GetTickCount();
            old_checkpoint = status.dwCheckPoint;
        }
        else
        {
            // Check if the timeout period has elapsed.
            // If so, report a timeout error and exit.
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

static void wait_status_pending(SC_HANDLE service, SC_HANDLE scm)
{
    // Structure to hold service status information.
    SERVICE_STATUS_PROCESS status;
    DWORD old_checkpoint;
    DWORD start_tick_count;
    DWORD wait_time;
    DWORD bytes_needed;

    // Query the status of the service.
    // Retrieves current status information about the specified service.
    if (!QueryServiceStatusEx(
        service,                        // Handle to the service
        SC_STATUS_PROCESS_INFO,         // Information level to query
        (LPBYTE)&status,                // Pointer to the SERVICE_STATUS_PROCESS structure
        sizeof(SERVICE_STATUS_PROCESS), // Size of the structure
        &bytes_needed))                 // Bytes needed if the buffer is too small
    {
        print_error("Failed to query service status");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return;
    }

    // Record the start time and initial checkpoint value.
    start_tick_count = GetTickCount();
    old_checkpoint = status.dwCheckPoint;

    // Loop while the service is in the "START_PENDING" state.
    while (status.dwCurrentState == SERVICE_START_PENDING)
    {
        // Calculate wait time. Wait no longer than the wait hint, constrained between 1 and 10 seconds.
        wait_time = status.dwWaitHint / 10;

        if (wait_time < 1000)
            wait_time = 1000;
        else if (wait_time > 10000)
            wait_time = 10000;

        Sleep(wait_time);

        // Query the status of the service again.
        if (!QueryServiceStatusEx(
            service,                     // Handle to the service
            SC_STATUS_PROCESS_INFO,     // Information level to query
            (LPBYTE)&status,             // Pointer to the SERVICE_STATUS_PROCESS structure
            sizeof(SERVICE_STATUS_PROCESS), // Size of the structure
            &bytes_needed))              // Bytes needed if the buffer is too small
        {
            print_error("Failed to query service status");
            break;
        }

        // If the checkpoint value has increased, reset the start time.
        // Indicates that the service is making progress.
        if (status.dwCheckPoint > old_checkpoint)
        {
            start_tick_count = GetTickCount();
            old_checkpoint = status.dwCheckPoint;
        }
        else
        {
            // If no progress is made within the wait hint period, break out of the loop.
            if (GetTickCount() - start_tick_count > status.dwWaitHint)
            {
                // No progress within the wait hint.
                break;
            }
        }
    }

    // Check if the service is running.
    // Report the final status of the service after waiting.
    if (status.dwCurrentState == SERVICE_RUNNING)
    {
        printf("Service started successfully.\n");
    }
    else
    {
        printf("Service not started.\n");
        printf("  Current State: %ld\n", status.dwCurrentState);
        printf("  Exit Code: %ld\n", status.dwWin32ExitCode);
        printf("  Check Point: %ld\n", status.dwCheckPoint);
        printf("  Wait Hint: %ld\n", status.dwWaitHint);
    }
}

static void start_service(void)
{
    // Open a handle to the Service Control Manager (SCM) with full access rights.
    // The SCM manages the services on the local machine.
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

    // Open a handle to the specified service with full access rights.
    // This handle is used to interact with the service (e.g., start, stop).
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        print_error("Failed to open service");
        CloseServiceHandle(scm);
        return;
    }

    // Ensure the service is not already running before attempting to start it.
    // Waits until the service is confirmed to be stopped.
    wait_status_stopped(service, scm);

    // Attempt to start the service.
    // The function StartService does not return until the service is either running or in a start-pending state.
    if (!StartService(service, 0, NULL)) {
        print_error("Failed to start service");
    }
    else {
        printf("Service start pending...\n");
    }

    // Wait for the service to be fully started.
    // This function will block until the service status indicates that it is running or fails to start.
    wait_status_pending(service, scm);

    // Close the handles to the service and SCM once operations are complete.
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

static void stop_service(void)
{
    // Open a handle to the Service Control Manager (SCM) with connect rights.
    // The SCM manages the services on the local machine.
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL) {
        print_error("Failed to open service manager");
        return;
    }

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

static void delete_service(void)
{
    // Open a handle to the Service Control Manager with the ability to create services.
    // This handle is necessary to interact with the service for deletion.
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL) {
        // Log an error if opening the service manager fails.
        print_error("Failed to open service manager");
        return;
    }

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
        stop_service();
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
        // If it fails, log the error and report the event.
        if (!StartServiceCtrlDispatcher(dispatch_table))
        {
            print_error("Failed StartServiceCtrlDispatcher");  // Log error if dispatcher fails
            svc_report_event(TEXT("StartServiceCtrlDispatcher"));  // Report the error to the event log
        }
        return 0;  // Exit the program after the service stops
    }

    // Command-line arguments are present, indicating a service management operation.
    // Perform the appropriate operation based on the argument provided.
    if (strcmp(argv[1], "install") == 0) {
        install_service();  // Install the service
    }
    else if (strcmp(argv[1], "start") == 0) {
        start_service();  // Start the service
    }
    else if (strcmp(argv[1], "stop") == 0) {
        stop_service();  // Stop the service
    }
    else if (strcmp(argv[1], "delete") == 0) {
        delete_service();  // Delete the service
    }
    else {
        // If the argument is invalid, print the usage message and return an error code.
        printf("Invalid option. Usage: svc <install|start|stop|delete>\n");
        return 1;
    }

    return 0;  // Exit the program successfully
}

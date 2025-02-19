#include "svc.h"

HANDLE g_process = NULL;

int main(int argc, char *argv[])
{
	// Run as a service if no arguments are provided.
	if (argc < 2)
	{
		SERVICE_TABLE_ENTRY dispatch_table[] = {
			{SVC_NAME, (LPSERVICE_MAIN_FUNCTION)svc_main},
			{NULL, NULL}};

		// Start the service control dispatcher.
		StartServiceCtrlDispatcher(dispatch_table);
		return 0;
	}

	// Open the Service Control Manager.
	SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (scm == NULL)
	{
		print_error("Failed to open service manager");
		return -1;
	}

	// Process the specified command.
	if (strcmp(argv[1], "install") == 0)
	{
		install_service(scm);
	}
	else if (strcmp(argv[1], "start") == 0)
	{
		start_service(scm);
	}
	else if (strcmp(argv[1], "stop") == 0)
	{
		stop_service(scm);
	}
	else if (strcmp(argv[1], "delete") == 0)
	{
		delete_service(scm);
	}
	else
	{
		printf("Invalid option. Usage: svc <install|start|stop|delete>\n");
		return 1;
	}

	return 0;
}

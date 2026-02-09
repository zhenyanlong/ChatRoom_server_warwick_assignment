#pragma once
#include <string>

// message command definitions
#define BROADCAST_MSG "!broadcast"
#define PRIVATE_MSG "!private"
#define USER_LIST_MSG "!userlist"
#define ADD_USER_MSG "!adduser"
#define REMOVE_USER_MSG "!removeuser"
#define EXIT_MSG "!exit"
#define UNKNOWN_MSG "!unknown"

static std::string GetStrBeforeFirstSymbol(const std::string& input_str, char symbol) {
	size_t first_space_pos = input_str.find(symbol);

	if (first_space_pos == std::string::npos) {
		return input_str;
	}
	else if (first_space_pos == 0) {
		
		return "";
	}
	else {
		return input_str.substr(0, first_space_pos);
	}
}

static void SplitStringAtFirstSpace(const std::string& input_str, std::string& before_space, std::string& after_space) {
	size_t first_space_pos = input_str.find(' ');
	if (first_space_pos == std::string::npos) {
		// No space found
		before_space = input_str;
		after_space = "";
	}
	else {
		before_space = input_str.substr(0, first_space_pos);
		after_space = input_str.substr(first_space_pos + 1); // +1 to skip the space
	}
}

// Add command header to message
static bool AddCommandHeader(std::string& message, const std::string& command) {
	if (command.empty() || command[0] != '!') {
		return false; // Command cannot be empty
	}
	message = command + " " + message;
	return true;
}

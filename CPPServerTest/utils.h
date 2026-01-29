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

static std::string GetStrBeforeFirstSpace(const std::string& input_str) {
	// 1. 查找第一个空格的位置（find返回size_t类型，未找到返回string::npos）
	size_t first_space_pos = input_str.find(' ');

	// 2. 处理不同情况
	if (first_space_pos == std::string::npos) {
		// 情况1：字符串中无空格，返回原字符串
		return input_str;
	}
	else if (first_space_pos == 0) {
		// 情况2：开头就是空格，返回空字符串（可根据需求调整，比如返回原串或报错）
		return "";
	}
	else {
		// 情况3：正常有空格，截取0到第一个空格位置的子串
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

// 为message添加命令头
static bool AddCommandHeader(std::string& message, const std::string& command) {
	if (command.empty() || command[0] != '!') {
		return false; // Command cannot be empty
	}
	message = command + " " + message;
	return true;
}

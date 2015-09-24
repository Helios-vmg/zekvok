#pragma once
#include "Utility.h"
#include "SimpleTypes.h"
#include "BackupSystem.h"

class LineProcessor{
	std::vector<std::string> args;
	std::set<std::string, strcmpci> argument_set;
	enum OperationMode{
		User,
		Pipe,
	};
	OperationMode operation_mode;
	std::unique_ptr<BackupSystem> backup_system;
	version_number_t selected_version;

#define DECLARE_PROCESS_OVERLOAD(x) void process_##x(const std::wstring *begin, const std::wstring *end)
	DECLARE_PROCESS_OVERLOAD(line);
	DECLARE_PROCESS_OVERLOAD(open);
	DECLARE_PROCESS_OVERLOAD(add);
	DECLARE_PROCESS_OVERLOAD(exclude);
	DECLARE_PROCESS_OVERLOAD(backup);
	DECLARE_PROCESS_OVERLOAD(restore);
	DECLARE_PROCESS_OVERLOAD(select);
	DECLARE_PROCESS_OVERLOAD(show);
	DECLARE_PROCESS_OVERLOAD(if);
	DECLARE_PROCESS_OVERLOAD(set);

#define DECLARE_PROCESS_EXCLUDE_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(exclude_##x)
	DECLARE_PROCESS_EXCLUDE_OVERLOAD(extension);
	DECLARE_PROCESS_EXCLUDE_OVERLOAD(path);
	DECLARE_PROCESS_EXCLUDE_OVERLOAD(name);
	
#define DECLARE_PROCESS_SELECT_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(select_##x)
	DECLARE_PROCESS_SELECT_OVERLOAD(version);

#define DECLARE_PROCESS_SHOW_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(show_##x)
	DECLARE_PROCESS_SHOW_OVERLOAD(dependencies);
	DECLARE_PROCESS_SHOW_OVERLOAD(versions);
	DECLARE_PROCESS_SHOW_OVERLOAD(version_count);
	DECLARE_PROCESS_SHOW_OVERLOAD(paths);

#define DECLARE_PROCESS_SET_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(set_##x)
	DECLARE_PROCESS_SET_OVERLOAD(use_snapshots);
	DECLARE_PROCESS_SET_OVERLOAD(change_criterium);
public:
	LineProcessor(int argc, char **argv);
	void process();
};

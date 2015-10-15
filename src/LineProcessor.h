/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class LineProcessor{
	std::vector<std::string> args;
	std::set<std::string, strcmpci> argument_set;
	enum class OperationMode{
		User,
		Pipe,
	};
	OperationMode operation_mode;
	std::unique_ptr<BackupSystem> backup_system;
	version_number_t selected_version;

	void ensure_backup_initialized();
	void ensure_existing_version();

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
	DECLARE_PROCESS_OVERLOAD(verify);
	DECLARE_PROCESS_OVERLOAD(generate);

#define DECLARE_PROCESS_EXCLUDE_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(exclude_##x)
	DECLARE_PROCESS_EXCLUDE_OVERLOAD(extension);
	DECLARE_PROCESS_EXCLUDE_OVERLOAD(path);
	DECLARE_PROCESS_EXCLUDE_OVERLOAD(name);
	
#define DECLARE_PROCESS_SELECT_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(select_##x)
	DECLARE_PROCESS_SELECT_OVERLOAD(version);
	DECLARE_PROCESS_SELECT_OVERLOAD(keypair);

#define DECLARE_PROCESS_SHOW_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(show_##x)
	DECLARE_PROCESS_SHOW_OVERLOAD(dependencies);
	DECLARE_PROCESS_SHOW_OVERLOAD(versions);
	DECLARE_PROCESS_SHOW_OVERLOAD(version_count);
	DECLARE_PROCESS_SHOW_OVERLOAD(paths);
	DECLARE_PROCESS_SHOW_OVERLOAD(version_summary);

#define DECLARE_PROCESS_SET_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(set_##x)
	DECLARE_PROCESS_SET_OVERLOAD(use_snapshots);
	DECLARE_PROCESS_SET_OVERLOAD(change_criterium);

#define DECLARE_PROCESS_GENERATE_OVERLOAD(x) DECLARE_PROCESS_OVERLOAD(generate_##x)
	DECLARE_PROCESS_GENERATE_OVERLOAD(keypair);
public:
	LineProcessor(int argc, char **argv);
	void process();
};

#include "stdafx.h"
#include "LineProcessor.h"
#include "BackupSystem.h"

LineProcessor::LineProcessor(int argc, char **argv):
		selected_version(-1){

	for (int i = 1; i < argc; i++)
		this->args.push_back(argv[i]);
	for (auto &i : this->args)
		this->argument_set.insert(i);

	this->operation_mode = this->argument_set.find("--pipe") != this->argument_set.end() ? OperationMode::Pipe : OperationMode::User;
}

std::vector<std::wstring> command_line_to_args(const std::string &line){
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	auto wline = conv.from_bytes(line);
	int argc;
	auto argv = CommandLineToArgvW(wline.c_str(), &argc);
	std::vector<std::wstring> ret;
	if (!argv)
		return ret;
	ret.reserve(argc);
	for (int i = 0; i < argc; i++)
		ret.push_back(argv[i]);
	LocalFree(argv);
	return ret;
}

void LineProcessor::process(){
	while (true){
		if (this->operation_mode == OperationMode::User)
			std::cout << "> ";
		std::string str_line;
		std::getline(std::cin, str_line);
		if (!std::cin)
			break;
		auto line = command_line_to_args(str_line);
		if (!line.size())
			continue;
		if (strcmpci::equal(line[0], L"quit"))
			break;
	}
}

template <typename T>
void iterate_pair_array(LineProcessor *This, const std::wstring *begin, const std::wstring *end, const T &array){
	for (auto &p : array){
		if (!strcmpci::equal(*begin, p.first))
			continue;
		if (++begin != end)
			(This->*p.second)(begin, end);
		break;
	}
}

void LineProcessor::process_line(const std::wstring *begin, const std::wstring *end){
	if (begin == end)
		return;

	static const std::pair<const wchar_t *, void (LineProcessor::*)(const std::wstring *begin, const std::wstring *end)> array[] = {
#define PROCESS_LINE_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_##x }
		PROCESS_LINE_ARRAY_ELEMENT(open),
		PROCESS_LINE_ARRAY_ELEMENT(add),
		PROCESS_LINE_ARRAY_ELEMENT(exclude),
		PROCESS_LINE_ARRAY_ELEMENT(backup),
		PROCESS_LINE_ARRAY_ELEMENT(restore),
		PROCESS_LINE_ARRAY_ELEMENT(select),
		PROCESS_LINE_ARRAY_ELEMENT(show),
		PROCESS_LINE_ARRAY_ELEMENT(if),
		PROCESS_LINE_ARRAY_ELEMENT(set),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_open(const std::wstring *begin, const std::wstring *end){
	this->backup_system.reset(new BackupSystem(*begin));
	this->selected_version = this->backup_system->get_version_count() - 1;
}

void LineProcessor::process_add(const std::wstring *begin, const std::wstring *end){
	if (!this->backup_system)
		return;
	this->backup_system->add_source(*begin);
}

void LineProcessor::process_exclude(const std::wstring *begin, const std::wstring *end){
	static const std::pair<const wchar_t *, void (LineProcessor::*)(const std::wstring *begin, const std::wstring *end)> array[] = {
#define PROCESS_EXCLUDE_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_exclude_##x }
		PROCESS_EXCLUDE_ARRAY_ELEMENT(extension),
		PROCESS_EXCLUDE_ARRAY_ELEMENT(path),
		PROCESS_EXCLUDE_ARRAY_ELEMENT(name),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_backup(const std::wstring *begin, const std::wstring *end){
	if (!this->backup_system)
		return;
	this->backup_system->perform_backup();
}

void LineProcessor::process_restore(const std::wstring *begin, const std::wstring *end){
	if (!this->backup_system)
		return;
	this->backup_system->restore_backup();
}

void LineProcessor::process_select(const std::wstring *begin, const std::wstring *end){
	static const std::pair<const wchar_t *, void (LineProcessor::*)(const std::wstring *begin, const std::wstring *end)> array[] = {
#define PROCESS_SELECT_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_select_##x }
		PROCESS_SELECT_ARRAY_ELEMENT(version),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_show(const std::wstring *begin, const std::wstring *end){
	static const std::pair<const wchar_t *, void (LineProcessor::*)(const std::wstring *begin, const std::wstring *end)> array[] = {
#define PROCESS_SHOW_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_show_##x }
		PROCESS_SHOW_ARRAY_ELEMENT(dependencies),
		PROCESS_SHOW_ARRAY_ELEMENT(versions),
		PROCESS_SHOW_ARRAY_ELEMENT(version_count),
		PROCESS_SHOW_ARRAY_ELEMENT(paths),
	};
	iterate_pair_array(this, begin, end, array);
}

std::string to_string(const std::wstring &s){
	std::string ret;
	ret.reserve(s.size());
	for (auto &c : s)
		ret.push_back((char)c);
	return ret;
}

void LineProcessor::process_if(const std::wstring *begin, const std::wstring *end){
	std::string narrowed = to_string(*begin);
	if (this->argument_set.find(narrowed) == this->argument_set.end())
		return;
	this->process_line(begin + 1, end);
}

void LineProcessor::process_set(const std::wstring *begin, const std::wstring *end){
	static const std::pair<const wchar_t *, void (LineProcessor::*)(const std::wstring *begin, const std::wstring *end)> array[] = {
#define PROCESS_SET_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_set_##x }
		PROCESS_SET_ARRAY_ELEMENT(use_snapshots),
		PROCESS_SET_ARRAY_ELEMENT(change_criterium),
	};
	iterate_pair_array(this, begin, end, array);
}

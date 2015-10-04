/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LineProcessor.h"
#include "BackupSystem.h"
#include "Exception.h"

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
		try{
			this->process_line(&line[0], &line[0] + line.size());
		}catch (NonFatalException &e){
			std::cerr << e.what() << std::endl;
		}
	}
}

template <typename T0, typename T1, typename T2>
struct triple{
	T0 first;
	T1 second;
	T2 third;
};

template <typename T>
void iterate_pair_array(LineProcessor *This, const std::wstring *begin, const std::wstring *end, const T &array){
	for (auto &p : array){
		if (!strcmpci::equal(*begin, p.first))
			continue;
		begin++;
		if (!p.third || begin != end)
			(This->*p.second)(begin, end);
		break;
	}
}

typedef triple<const wchar_t *, void (LineProcessor::*)(const std::wstring *begin, const std::wstring *end), int> process_array_t;

void LineProcessor::process_line(const std::wstring *begin, const std::wstring *end){
	if (begin == end)
		return;

	static const process_array_t array[] = {
#define PROCESS_LINE_ARRAY_ELEMENT(x, y) { L###x , &LineProcessor::process_##x, y }
		PROCESS_LINE_ARRAY_ELEMENT(open, 1),
		PROCESS_LINE_ARRAY_ELEMENT(add, 1),
		PROCESS_LINE_ARRAY_ELEMENT(exclude, 1),
		PROCESS_LINE_ARRAY_ELEMENT(backup, 0),
		PROCESS_LINE_ARRAY_ELEMENT(restore, 0),
		PROCESS_LINE_ARRAY_ELEMENT(select, 1),
		PROCESS_LINE_ARRAY_ELEMENT(show, 1),
		PROCESS_LINE_ARRAY_ELEMENT(if, 1),
		PROCESS_LINE_ARRAY_ELEMENT(set, 1),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_open(const std::wstring *begin, const std::wstring *end){
	this->backup_system.reset(new BackupSystem(*begin));
	this->selected_version = this->backup_system->get_version_count() - 1;
}

void LineProcessor::process_add(const std::wstring *begin, const std::wstring *end){
	this->ensure_backup_initialized();
	this->backup_system->add_source(*begin);
}

void LineProcessor::process_exclude(const std::wstring *begin, const std::wstring *end){
	static const process_array_t array[] = {
#define PROCESS_EXCLUDE_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_exclude_##x, 1 }
		PROCESS_EXCLUDE_ARRAY_ELEMENT(extension),
		PROCESS_EXCLUDE_ARRAY_ELEMENT(path),
		PROCESS_EXCLUDE_ARRAY_ELEMENT(name),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_backup(const std::wstring *begin, const std::wstring *end){
	this->ensure_backup_initialized();
	this->backup_system->perform_backup();
}

void LineProcessor::process_restore(const std::wstring *begin, const std::wstring *end){
	this->ensure_backup_initialized();
	this->backup_system->restore_backup();
}

void LineProcessor::process_select(const std::wstring *begin, const std::wstring *end){
	static const process_array_t array[] = {
#define PROCESS_SELECT_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_select_##x, 1 }
		PROCESS_SELECT_ARRAY_ELEMENT(version),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_show(const std::wstring *begin, const std::wstring *end){
	static const process_array_t array[] = {
#define PROCESS_SHOW_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_show_##x, 0 }
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
	static const process_array_t array[] = {
#define PROCESS_SET_ARRAY_ELEMENT(x) { L###x , &LineProcessor::process_set_##x, 1 }
		PROCESS_SET_ARRAY_ELEMENT(use_snapshots),
		PROCESS_SET_ARRAY_ELEMENT(change_criterium),
	};
	iterate_pair_array(this, begin, end, array);
}

void LineProcessor::process_exclude_extension(const std::wstring *begin, const std::wstring *end){
	this->ensure_backup_initialized();
	this->backup_system->add_ignored_extension(*begin);
}

void LineProcessor::process_exclude_path(const std::wstring *begin, const std::wstring *end){
	this->ensure_backup_initialized();
	this->backup_system->add_ignored_path(*begin);
}

void LineProcessor::process_exclude_name(const std::wstring *begin, const std::wstring *end){
	if (begin + 1 == end)
		return;
	const wchar_t *strings[] = {
		L"files",
		L"dirs",
		L"all",
	};
	for (auto i = array_size(strings); i--; ){
		if (!strcmpci::equal(*begin, strings[i]))
			continue;
		this->ensure_backup_initialized();
		this->backup_system->add_ignored_name(begin[1], (NameIgnoreType)(i + 1));
		break;
	}
}

void LineProcessor::ensure_backup_initialized(){
	if (!this->backup_system)
		throw std::exception("No backup selected.");
}

void LineProcessor::ensure_existing_version(){
	this->ensure_backup_initialized();
	if (!this->backup_system->get_version_count())
		throw std::exception("Backup has never been performed.");
}

void LineProcessor::process_select_version(const std::wstring *begin, const std::wstring *end){
	std::wstringstream stream(*begin);
	version_number_t version;
	if (!(stream >> version))
		return;
	this->ensure_existing_version();
	if (version < 0)
		version += this->backup_system->get_version_count();
	if (!this->backup_system->version_exists(version))
		throw std::exception("No such version in backup.");
	this->selected_version = version;
}

template <typename T>
std::ostream &operator<<(std::ostream &stream, const std::vector<T> &v){
	bool first = true;
	for (auto &i : v){
		if (!first)
			stream << ", ";
		else
			first = false;
		stream << i;
	}
	return stream;
}

void LineProcessor::process_show_dependencies(const std::wstring *begin, const std::wstring *end){
	if (this->operation_mode != OperationMode::User)
		return;

	this->ensure_existing_version();
	auto deps = this->backup_system->get_version_dependencies(this->selected_version);
	std::cout
		<< "Dependencies: " << deps << std::endl
		<< "Total: " << deps.size() << std::endl;
}

void LineProcessor::process_show_versions(const std::wstring *begin, const std::wstring *end){
	if (this->operation_mode != OperationMode::User)
		return;

	this->ensure_backup_initialized();
	auto &vs = this->backup_system->get_versions();
	std::cout << "Available versions: " << vs << std::endl;
}

void LineProcessor::process_show_version_count(const std::wstring *begin, const std::wstring *end){
	if (this->operation_mode != OperationMode::User)
		return;

	this->ensure_backup_initialized();
	if (!this->backup_system->get_version_count())
		std::cout << "No versions.\n";
	else{
		auto &vs = this->backup_system->get_versions();
		std::cout
			<< "Available versions count: " << vs.size() << std::endl
			<< "Latest version: " << vs.back() << std::endl;
	}
}

std::wostream &operator<<(std::wostream &stream, FileSystemObjectType type){
	switch (type){
#define WOUTPUT_FileSystemObjectType_CASE(x) case FileSystemObjectType::x: return stream << L###x
		WOUTPUT_FileSystemObjectType_CASE(Directory);
		WOUTPUT_FileSystemObjectType_CASE(RegularFile);
		WOUTPUT_FileSystemObjectType_CASE(DirectorySymlink);
		WOUTPUT_FileSystemObjectType_CASE(Junction);
		WOUTPUT_FileSystemObjectType_CASE(FileSymlink);
		WOUTPUT_FileSystemObjectType_CASE(FileReparsePoint);
		WOUTPUT_FileSystemObjectType_CASE(FileHardlink);
	}
}

std::string format_size(double size){
	static const char *units[] = {
		" B",
		" KiB",
		" MiB",
		" GiB",
		" TiB",
		" PiB",
		" EiB",
		" ZiB",
		" YiB"
	};
	int unit = 0;
	bool exact = true;
	while (size >= 1024.0){
		exact &= fmod(size, 1024.0) == 0;
		size /= 1024.0;
		unit++;
	}
	std::stringstream stream;
	if (!exact)
		stream << std::fixed << std::setprecision(1);
	stream << size << units[unit];
	return stream.str();
}

void LineProcessor::process_show_paths(const std::wstring *begin, const std::wstring *end){
	if (this->operation_mode != OperationMode::User)
		return;
	this->ensure_backup_initialized();
	auto entries = this->backup_system->get_entries(this->selected_version);
	size_t entry_id = 0;
	for (auto &entry : entries){
		std::wcout << L"Entry " << entry_id << L", base: ";
		auto base_path = entry->get_mapped_base_path();
		if (!base_path)
			std::wcout << L"(null)";
		else
			std::wcout << *base_path;
		std::wcout << std::endl;

		for (auto &fso : entry->get_iterator()){
			std::wcout <<
				fso->get_path_without_base().wstring() << L"\n"
				L"    Type: " << fso->get_type() << std::endl;
			if (fso->is_pseudoregular_file())
				std::cout << L"    Size: " << format_size((double)fso->get_size()) << std::endl;
			auto stream_id = fso->get_stream_id();
			if (stream_id != invalid_stream_id)
				std::cout <<
					"    Stream ID: " << stream_id << "\n"
					"    Stored in version: " << fso->get_latest_version() << "\n";
			
			if (fso->is_linkish() && fso->get_type() != FileSystemObjectType::FileHardlink)
				std::wcout << L"    Link target: " << *fso->get_link_target();
		}
	}
}

void LineProcessor::process_set_use_snapshots(const std::wstring *begin, const std::wstring *end){
	this->ensure_backup_initialized();
	if (strcmpci::equal(*begin, L"true"))
		this->backup_system->set_use_snapshots(true);
	else if (strcmpci::equal(*begin, L"false"))
		this->backup_system->set_use_snapshots(false);
}

void LineProcessor::process_set_change_criterium(const std::wstring *begin, const std::wstring *end){
	const wchar_t *strings[] = {
		L"archive_flag",
		L"size",
		L"date",
		L"hash",
		L"hash_auto",
	};
	for (auto i = array_size(strings); i--; ){
		if (!strcmpci::equal(*begin, strings[i]))
			continue;
		this->ensure_backup_initialized();
		this->backup_system->set_change_criterium((ChangeCriterium)i);
		break;
	}
}

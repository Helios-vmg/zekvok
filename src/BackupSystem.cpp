/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "BackupSystem.h"
#include "ArchiveIO.h"
#include "System/SystemOperations.h"
#include "System/VSS.h"

class SimpleErrorReporter : public FileSystemObject::ErrorReporter{
public:
	bool report_error(std::exception &e, const char *context) override{
		std::cerr << "Exception was thrown";
		if (context)
			std::cerr << " while " << context;
		std::cerr << ": " << e.what() << std::endl;
		return true;
	}
};

namespace fs = boost::filesystem;

BackupSystem::BackupSystem(const std::wstring &dst):
		version_count(-1),
		use_snapshots(true),
		change_criterium(ChangeCriterium::Default),
		base_objects_set(false){
	this->target_path = dst;
	if (fs::exists(this->target_path)){
		if (!fs::is_directory(this->target_path))
			throw std::exception("Target path exists and is not a directory.");
	}else
		fs::create_directory(this->target_path);
	
	this->set_versions();
}

const boost::match_flag_type default_regex_flags = (boost::match_flag_type)(boost::match_default | boost::format_perl | boost::regex::icase);

void BackupSystem::set_versions(){
	boost::wregex re(L".*\\\\?version([0-9]+)\\.arc", default_regex_flags);
	fs::directory_iterator i(this->target_path),
		e;
	for (; i != e; ++i){
		auto path = i->path().wstring();
		auto start = path.begin(),
			end = path.end();
		boost::match_results<decltype(start)> match;
		if (!boost::regex_search(start, end, match, re, default_regex_flags))
			continue;
		auto s = std::wstring(match[1].first,match[1].second);
		std::wstringstream stream(s);
		version_number_t v;
		stream >> v;
		this->versions.push_back(v);
	}
	std::sort(this->versions.begin(), this->versions.end());
}

version_number_t BackupSystem::get_version_count(){
	if (this->version_count >= 0)
		return this->version_count;
	return this->version_count = this->versions.size() ? this->versions.back() + 1 : 0;
}

void BackupSystem::add_source(const std::wstring &src){
	this->sources.push_back(src);
}

void BackupSystem::add_ignored_extension(const std::wstring &ext){
	this->ignored_extensions.insert(ext);
}

void BackupSystem::add_ignored_path(const std::wstring &path){
	this->ignored_paths.insert(path);
}

void BackupSystem::add_ignored_name(const std::wstring &name, NameIgnoreType type){
	this->ignored_names[name] = type;
}

bool BackupSystem::version_exists(version_number_t v) const{
	auto b = this->versions.begin(),
		e = this->versions.end();
	auto i = std::lower_bound(b, e, v, [](version_number_t a, version_number_t b){ return a < b; });
	return i != e;
}

path_t BackupSystem::get_version_path(version_number_t version) const{
	path_t ret = this->target_path;
	std::wstringstream stream;
	stream << L"version" << std::setw(8) << std::setfill(L'0') << version << L".arc";
	ret /= stream.str();
	return ret;
}

std::vector<version_number_t> BackupSystem::get_version_dependencies(version_number_t version) const{
	ArchiveReader reader(this->get_version_path(version));
	return reader.read_manifest()->version_dependencies;
}

void BackupSystem::set_use_snapshots(bool use_snapshots){
	this->use_snapshots = use_snapshots;
}

void BackupSystem::set_change_criterium(ChangeCriterium cc){
	this->change_criterium = cc;
}

bool is_backupable(system_ops::DriveType type){
	switch (type)
    {
        case system_ops::DriveType::Unknown:
        case system_ops::DriveType::NoRootDirectory:
        case system_ops::DriveType::Network:
        case system_ops::DriveType::CDRom:
            return false;
        case system_ops::DriveType::Removable:
        case system_ops::DriveType::Fixed:
        case system_ops::DriveType::Ram:
            return true;
    }
	return false;
}

void BackupSystem::perform_backup(){
	auto start_time = OpaqueTimestamp::utc_now();
	for (auto &vi : system_ops::enumerate_volumes()){
		if (is_backupable(vi.drive_type))
			continue;
		this->current_volumes[vi.volume_path] = vi;
	}
	if (!this->use_snapshots){
		this->perform_backup_inner(start_time);
		return;
	}
	std::cout << "Creating shadows.\n";
	VssSnapshot snapshot(get_map_keys(this->current_volumes));
	for (auto &shadow : snapshot.get_snapshot_properties().get_shadows()){
		start_time = shadow.created_at;
		break;
	}
	this->set_path_mapper(snapshot);
	this->perform_backup_inner(start_time);
}

template <typename T>
std::basic_string<T> regex_escape(const std::basic_string<T> &s){
	boost::basic_regex<T> esc(to_string<T>("[.^$|()\\[\\]{}*+?\\\\]"));
	auto rep = to_string<T>("\\\\$&");
	return regex_replace(url_to_escape, esc, rep, boost::match_default | boost::format_perl);
}

void map_path(const std::wstring &A, const std::wstring &B, std::vector<std::pair<boost::wregex, std::wstring>> &mapper){
	std::wstring pattern;
	pattern += L"(";
	pattern += A;
	pattern += L")(\\.*|$)";
	boost::wregex re(pattern, default_regex_flags);
	mapper.push_back(std::make_pair(re, B));
}

void BackupSystem::set_path_mapper(const VssSnapshot &snapshot){
	this->path_mapper.clear();
	this->reverse_path_mapper.clear();
	for (auto &shadow : snapshot.get_snapshot_properties().get_shadows()){
		auto volume = this->current_volumes[shadow.original_volume_name];
		auto s = ensure_last_character_is_not_backslash(shadow.snapshot_device_object);
		for (auto &path : volume.mounted_paths){
			auto s2 = ensure_last_character_is_not_backslash(path);
			map_path(s2, s, this->path_mapper);
			map_path(s, s2, this->reverse_path_mapper);
		}
	}
}

void BackupSystem::perform_backup_inner(const OpaqueTimestamp &start_time){
	std::cout << "Performing backup.\n";
	if (!this->get_version_count())
		this->create_initial_version(start_time);
	else
		this->create_new_version(start_time);
}

void BackupSystem::create_initial_version(const OpaqueTimestamp &start_time){
	this->set_base_objects();
	this->generate_first_archive(start_time);
}

void BackupSystem::generate_first_archive(const OpaqueTimestamp &start_time){
	this->generate_archive(start_time, &BackupSystem::generate_initial_stream);
}

std::function<BackupMode(const FileSystemObject &)> BackupSystem::make_map(const std::shared_ptr<std::vector<std::wstring>> &for_later_check){
	return [for_later_check, this](const FileSystemObject &fso){
		bool follow;
		auto ret = this->get_backup_mode_for_object(fso, follow);
		if (follow && fso.get_link_target() && fso.get_link_target()->size()){
			//follow_link_targets not yet implemented!
			abort();
			for_later_check->push_back(*fso.get_link_target());
		}
		return ret;
	};
}

std::vector<path_t> BackupSystem::get_current_source_locations(){
	if (!this->get_version_count())
		return this->sources;
	std::vector<path_t> ret;
	for (auto &fso : this->old_objects){
		if (!fso->get_is_main() || !fso->get_mapped_base_path())
			continue;
		ret.push_back(path_t(*fso->get_mapped_base_path()));
	}
	return ret;
}

void BackupSystem::set_base_objects(){
	if (this->base_objects_set)
		return;
	this->base_objects_set = true;

	std::shared_ptr<std::vector<std::wstring>> for_later_check(new std::vector<std::wstring>);
	FileSystemObject::CreationSettings settings = {
		std::shared_ptr<FileSystemObject::ErrorReporter>(new SimpleErrorReporter),
		this->make_map(for_later_check)
	};
	for (auto &current_source_location : this->get_current_source_locations()){
		auto mapped = this->map_forward(current_source_location);
		std::shared_ptr<FileSystemObject> fso(FileSystemObject::create(mapped, current_source_location, settings));
		fso->set_is_main(true);
		this->base_objects.push_back(fso);
	}

	while (for_later_check->size()){
		auto old_for_later_check = for_later_check;
		for_later_check.reset(new std::vector<std::wstring>);
		settings.backup_mode_map = this->make_map(for_later_check);
		for (auto &path : *old_for_later_check){
			if (this->covered(path))
				continue;
			std::shared_ptr<FileSystemObject> fso(
				FileSystemObject::create(
					this->map_forward(path),
					path,
					settings
				)
			);
			this->base_objects.push_back(fso);
		}
	}

	int i = 0;
	for (auto &fso : this->base_objects)
		fso->set_entry_number(i++);

	this->recalculate_file_guids();
}

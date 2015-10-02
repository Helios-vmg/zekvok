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
#include "SymbolicConstants.h"
#include "serialization_utils.h"
#include "Utility.h"

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
		base_objects_set(false),
		next_stream_id(first_valid_stream_id),
		next_differential_chain_id(first_valid_differential_chain_id){
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
			::map_path(s2, s, this->path_mapper);
			::map_path(s, s2, this->reverse_path_mapper);
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

void BackupSystem::generate_archive(const OpaqueTimestamp &start_time, generate_archive_fp generator, version_number_t version){
	auto first_stream_id = this->next_stream_id;
	auto first_diff_id = this->next_differential_chain_id;
	auto version_path = this->get_version_path(version);

	ArchiveWriter archive(version_path);
	auto stream_dict = this->generate_streams(generator);
	std::set<version_number_t> version_dependencies;

	std::unique_ptr<ArchiveWriter_helper> array[] = {
		make_unique(new ArchiveWriter_helper_first(
			make_shared(new ArchiveWriter_helper::first_co_t::pull_type(
				[&](ArchiveWriter_helper::first_co_t::push_type &sink){
					for (auto &kv : stream_dict){
						{
							auto base_object = this->base_objects[kv.first];
							for (auto &i : kv.second)
								for (auto &j : i->get_file_system_objects())
									j->set_backup_stream(i.get());

							for (auto &i : base_object->get_iterator())
								this->get_dependencies(version_dependencies, *i);
						}

						for (auto &backup_stream : kv.second){
							auto fso = backup_stream->get_file_system_objects()[0];
							std::wcout << *fso->get_unmapped_base_path() << std::endl;
							bool compute = !fso->get_hash().valid;
							sha256_digest digest;
							std::uint64_t size;
							auto stream = backup_stream->open_for_exclusive_read(size);
							ArchiveWriter_helper::first_push_t s = {
								&digest,
								backup_stream->get_unique_id(),
								stream.get(),
								size,
							};
							sink(s);
							fso->set_hash(digest);
						}
					}
				}
			))
		)),
		make_unique(new ArchiveWriter_helper_second(
			make_shared(new ArchiveWriter_helper::second_co_t::pull_type(
				[&](ArchiveWriter_helper::second_co_t::push_type &sink){
					for (auto &kv : stream_dict)
						sink(this->base_objects[kv.first].get());
				}
			))
		)),
		make_unique(new ArchiveWriter_helper_third(
			make_shared(new ArchiveWriter_helper::third_co_t::pull_type(
				[&](ArchiveWriter_helper::third_co_t::push_type &sink){
					VersionManifest manifest;
					manifest.creation_time = start_time;
					manifest.version_number = version;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#endif
					manifest.entry_count = base_objects.size();
					manifest.first_stream_id = first_stream_id;
					manifest.first_differential_chain_id = first_diff_id;
					manifest.next_stream_id = this->next_stream_id;
					manifest.next_differential_chain_id = this->next_differential_chain_id;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
					manifest.version_dependencies = to_vector<typename decltype(manifest.version_dependencies)::value_type>(version_dependencies);
					sink(&manifest);
				}
			))
		)),
	};

	archive.process(array, array + array_size(array));
}

std::shared_ptr<BackupStream> BackupSystem::generate_initial_stream(FileSystemObject &fso, known_guids_t &known_guids){
	if (!this->should_be_added(fso, known_guids)){
		this->fix_up_stream_reference(fso, known_guids);
		return std::shared_ptr<BackupStream>();
	}
	auto ret = make_shared(new FullStream);
	ret->set_unique_id(fso.get_stream_id());
	ret->set_physical_size(fso.get_size());
	ret->set_virtual_size(fso.get_size());
	if (fso.get_file_system_guid().valid)
		known_guids[fso.get_file_system_guid().data] = ret;
	ret->add_file_system_object(&fso);
	return ret;
}

bool BackupSystem::should_be_added(FileSystemObject &fso, known_guids_t &known_guids){
	fso.set_latest_version(-1);
	if (fso.is_directoryish())
		return false;
	if (fso.get_backup_mode() == BackupMode::NoBackup)
		return false;
	if (fso.is_linkish() && fso.get_type() == FileSystemObjectType::FileHardlink)
		return false;
	auto &guid = fso.get_file_system_guid();
	if (guid.valid && known_guids.find(guid.data) != known_guids.end())
		return false;
	return true;
}

BackupMode BackupSystem::get_backup_mode_for_object(const FileSystemObject &fso, bool &follow_link_targets){
	follow_link_targets = false;
	bool dir = fso.is_directoryish();
	auto it = this->ignored_names.find(fso.get_name());
	if (it != this->ignored_names.end()){
		auto ignore_type = it->second;
		if (((unsigned)ignore_type & (unsigned)NameIgnoreType::File) && !dir)
			return BackupMode::NoBackup;
		if (((unsigned)ignore_type & (unsigned)NameIgnoreType::Directory) && dir)
			return BackupMode::NoBackup;
	}
	if (this->ignored_extensions.find(path_t(fso.get_name()).extension().wstring()) != this->ignored_extensions.end() && !dir)
		return BackupMode::NoBackup;
	if (this->ignored_paths.find(*fso.get_unmapped_base_path()) != this->ignored_paths.end())
		return BackupMode::NoBackup;
	return dir ? BackupMode::Directory : BackupMode::Full;
}

path_t BackupSystem::map_forward(const path_t &path){
	return this->map_path(path, this->path_mapper);
}

path_t BackupSystem::map_back(const path_t &path){
	return this->map_path(path, this->reverse_path_mapper);
}

path_t BackupSystem::map_path(const path_t &path, const path_mapper_t &mapper){
	path_t ret;
	stream_index_t longest = 0;
	for (auto &tuple : mapper){
		auto temp = path.wstring();
		auto start = temp.begin(),
			end = temp.end();
		boost::match_results<decltype(start)> match;
		auto &re = tuple.first;
		if (!boost::regex_search(start, end, match, re, default_regex_flags))
			continue;
		std::wstring g1(match[1].first, match[1].second);
		if (g1.size() <= longest)
			continue;
		longest = g1.size();
		std::wstring g2(match[2].first, match[2].second);
		ret = tuple.second + g2;
	}
	return !longest ? path : ret;
}

bool BackupSystem::covered(const path_t &path){
	for (auto &fso : this->base_objects)
		if (fso->contains(path))
			return true;
	return false;
}

void BackupSystem::recalculate_file_guids(){
	while (this->recalculate_file_guids_queue.size()){
		auto fso = this->recalculate_file_guids_queue.front();
		this->recalculate_file_guids_queue.pop_front();
		auto path = this->map_back(fso->get_path());
		fso->set_file_system_guid(path, false);
	}
}

BackupSystem::stream_dict_t BackupSystem::generate_streams(generate_archive_fp generator){
	known_guids_t known_guids;
	stream_dict_t stream_dict;
	for (stream_index_t i = 0; i < this->base_objects.size(); i++){
		auto fso = this->base_objects[i];
		std::vector<std::shared_ptr<BackupStream>> streams;
		for (auto &child : fso->get_iterator()){
			auto stream = (this->*generator)(*child, known_guids);
			if (!stream || !stream->has_data())
				continue;
			child->set_unique_ids(this);
			stream->set_unique_id(child->get_stream_id());
			streams.push_back(stream);
			this->streams.push_back(stream);
		}
		stream_dict[i] = std::move(streams);
	}
	return stream_dict;
}

void BackupSystem::get_dependencies(std::set<version_number_t> &dst, FileSystemObject &fso) const{
	if (fso.get_latest_version() == invalid_version_number)
		return;
	if (!fso.get_backup_stream())
		dst.insert(fso.get_latest_version());
	else
		fso.get_backup_stream()->get_dependencies(dst);
}

void BackupSystem::fix_up_stream_reference(FileSystemObject &fso, known_guids_t &known_guids){
	if (fso.get_type() == FileSystemObjectType::FileHardlink)
		return;
	auto &guid = fso.get_file_system_guid();
	if (guid.valid)
		return;
	auto it = known_guids.find(guid.data);
	if (it == known_guids.end())
		return;
	auto stream = it->second;
	fso.set_stream_id(stream->get_unique_id());
	fso.set_backup_stream(stream.get());
	auto &fsos = stream->get_file_system_objects();
	if (fsos.size())
		fso.set_latest_version(fsos.front()->get_latest_version());
	stream->add_file_system_object(&fso);
}

std::wstring simplify_path(const std::wstring &);

void BackupSystem::create_new_version(const OpaqueTimestamp &start_time){
	{
		ArchiveReader archive(this->get_version_path(this->versions.back()));
		auto manifest = archive.read_manifest();
		this->old_objects = archive.get_base_objects();
		this->next_stream_id = manifest->next_stream_id;
		this->next_differential_chain_id = manifest->next_differential_chain_id;
	}
	this->set_old_objects_map();
	this->set_base_objects();
	for (auto &base_object : this->base_objects){
		for (auto &fso : base_object->get_iterator()){
			auto it = this->old_objects_map.find(simplify_path(*fso->get_mapped_base_path()));
			if (it == this->old_objects_map.end())
				continue;
			fso->set_stream_id(it->second->get_stream_id());
			break;
		}
	}
	this->generate_archive(start_time, &BackupSystem::check_and_maybe_add, this->get_new_version_number());
}

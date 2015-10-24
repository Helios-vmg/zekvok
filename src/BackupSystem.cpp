/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"

const wchar_t * const system_path_prefix = L"\\\\?\\";

class SimpleErrorReporter : public FileSystemObject::ErrorReporter{
public:
	bool report_error(const std::exception &e, const char *context) override{
		std::cerr << "Exception was thrown";
		if (context)
			std::cerr << " while " << context;
		std::cerr << ": " << e.what() << std::endl;
		return true;
	}
	bool report_win32_error(std::uint32_t error, const char *context) override{
		std::cerr << "Win32 error was triggered ";
		if (context)
			std::cerr << "while " << context;
		std::cerr << ": " << error << std::endl;
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
	if (version <= invalid_version_number)
		throw std::exception("Invalid version number.");
	path_t ret = this->target_path;
	std::wstringstream stream;
	stream << L"version" << std::setw(8) << std::setfill(L'0') << version << L".arc";
	ret /= stream.str();
	return ret;
}

std::vector<version_number_t> BackupSystem::get_version_dependencies(version_number_t version) const{
	ArchiveReader reader(this->get_version_path(version), this->keypair.get());
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
		if (!is_backupable(vi.drive_type))
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
	static const char escaped_characters[] = ".^$|()[]{}*+?\\";
	std::basic_string<T> ret;
	for (T c : s){
		bool found = false;
		for (char c2 : escaped_characters){
			if (c == c2){
				ret += '\\';
				ret += c2;
				found = true;
				break;
			}
		}
		if (found)
			continue;
		ret += c;
	}
	return ret;
}

void map_path(const std::wstring &A, const std::wstring &B, std::vector<std::pair<boost::wregex, std::wstring>> &mapper){
	std::wstring pattern;
	pattern += L"(";
	pattern += regex_escape(A);
	pattern += L")(\\\\.*$|$)";
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
		if (!fso->get_is_main())
			continue;
		ret.push_back(fso->get_mapped_path());
	}
	return ret;
}

void BackupSystem::set_base_objects(){
	if (this->base_objects_set)
		return;
	this->base_objects_set = true;

	std::shared_ptr<std::vector<std::wstring>> for_later_check(new std::vector<std::wstring>);
	FileSystemObject::CreationSettings settings = {
		this,
		make_shared(new SimpleErrorReporter),
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

	KernelTransaction tx;

	{
		ArchiveWriter archive(tx, version_path, this->keypair.get());
		auto stream_dict = this->generate_streams(generator);
		std::set<version_number_t> version_dependencies;

		std::unique_ptr<ArchiveWriter_helper> array[] = {
			make_unique(new ArchiveWriter_helper_first(
				make_shared(new ArchiveWriter_helper::first_co_t::pull_type(
					[&](ArchiveWriter_helper::first_co_t::push_type &sink){
						sink({nullptr, 0, nullptr, 0});
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
								std::wcout << fso->get_unmapped_path() << std::endl;
								bool compute = !fso->get_hash().valid;
								sha256_digest digest;
								std::uint64_t size;
								auto stream = fso->open_for_exclusive_read(size);
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
						sink(nullptr);
						for (auto &kv : stream_dict)
							sink(this->base_objects[kv.first].get());
					}
				))
			)),
			make_unique(new ArchiveWriter_helper_third(
				make_shared(new ArchiveWriter_helper::third_co_t::pull_type(
					[&](ArchiveWriter_helper::third_co_t::push_type &sink){
						sink(nullptr);
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

	this->save_encrypted_base_objects(tx, version);
}

void BackupSystem::save_encrypted_base_objects(KernelTransaction &tx, version_number_t version){
	auto dst = this->target_path;
	dst /= ".aux";
	if (!boost::filesystem::exists(dst))
		boost::filesystem::create_directory(dst);
	else if (!boost::filesystem::is_directory(dst))
		return;
	
	{
		std::stringstream temp;
		temp << "fso" << std::setw(8) << std::setfill('0') << version << ".dat";
		dst /= temp.str();
	}
	boost::iostreams::stream<TransactedFileSink> file(tx, dst.wstring().c_str());
	for (auto &fso : this->base_objects){
		auto cloned = easy_clone(*fso);
		cloned->encrypt();
		buffer_t mem;
		{
			boost::iostreams::stream<MemorySink> omem(&mem);
			SerializerStream stream(omem);
			stream.begin_serialization(*cloned);
		}

		simple_buffer_serialization(file, mem);
	}
}

std::shared_ptr<BackupStream> BackupSystem::generate_initial_stream(FileSystemObject &fso, known_guids_t &known_guids){
	if (!this->should_be_added(fso, known_guids)){
		this->fix_up_stream_reference(fso, known_guids);
		return std::shared_ptr<BackupStream>();
	}
	auto &filish = static_cast<FilishFso &>(fso);
	auto ret = make_shared(new FullStream);
	ret->set_unique_id(filish.get_stream_id());
	ret->set_physical_size(filish.get_size());
	ret->set_virtual_size(filish.get_size());
	if (filish.get_file_system_guid().valid)
		known_guids[filish.get_file_system_guid().data] = ret;
	ret->add_file_system_object(&filish);
	return ret;
}

bool BackupSystem::should_be_added(FileSystemObject &fso, known_guids_t &known_guids){
	fso.set_latest_version(-1);
	if (fso.is_directoryish())
		return false;
	auto &filish = static_cast<FilishFso &>(fso);
	if (filish.get_backup_mode() == BackupMode::NoBackup)
		return false;
	if (filish.is_linkish() && filish.get_type() == FileSystemObjectType::FileHardlink)
		return false;
	fso.set_latest_version(this->get_new_version_number());
	auto &guid = filish.get_file_system_guid();
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
		auto path = this->map_back(fso->get_mapped_path());
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
			child->set_unique_ids(*this);
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
	if (fso.get_type() == FileSystemObjectType::FileHardlink || fso.is_directoryish())
		return;
	auto &filish = static_cast<FilishFso &>(fso);
	auto &guid = filish.get_file_system_guid();
	if (guid.valid)
		return;
	auto it = known_guids.find(guid.data);
	if (it == known_guids.end())
		return;
	auto stream = it->second;
	filish.set_stream_id(stream->get_unique_id());
	filish.set_backup_stream(stream.get());
	auto &fsos = stream->get_file_system_objects();
	if (fsos.size())
		filish.set_latest_version(fsos.front()->get_latest_version());
	stream->add_file_system_object(&filish);
}

std::wstring normalize_path(const std::wstring &path){
	std::wstring ret;
	size_t skip = 0;
	if (starts_with(path, system_path_prefix)){
		ret = system_path_prefix;
		ret += path_t(path.substr(4)).normalize().wstring();
	}
	return ret;
}

std::wstring simplify_path(const std::wstring &path){
	return to_lower(normalize_path(path));
}

void BackupSystem::create_new_version(const OpaqueTimestamp &start_time){
	{
		ArchiveReader archive(this->get_version_path(this->versions.back()), this->keypair.get());
		auto manifest = archive.read_manifest();
		this->old_objects = archive.get_base_objects();
		this->next_stream_id = manifest->next_stream_id;
		this->next_differential_chain_id = manifest->next_differential_chain_id;
	}
	this->set_old_objects_map();
	this->set_base_objects();
	for (auto &base_object : this->base_objects){
		for (auto &fso : base_object->get_iterator()){
			auto it = this->old_objects_map.find(simplify_path(fso->get_mapped_path().wstring()));
			if (it == this->old_objects_map.end())
				continue;
			fso->set_stream_id(it->second->get_stream_id());
			break;
		}
	}
	this->generate_archive(start_time, &BackupSystem::check_and_maybe_add, this->get_new_version_number());
}

void BackupSystem::set_old_objects_map(){
	this->old_objects_map.clear();
	for (auto &old_object : this->old_objects)
		for (auto &fso : old_object->get_iterator())
			this->old_objects_map[simplify_path(fso->get_mapped_path().wstring())] = fso;
}

std::shared_ptr<BackupStream> BackupSystem::check_and_maybe_add(FileSystemObject &fso, known_guids_t &known_guids){
	std::shared_ptr<BackupStream> ret;
	if (!this->should_be_added(fso, known_guids)){
		this->fix_up_stream_reference(fso, known_guids);
		return ret;
	}
	auto &filish = static_cast<FilishFso &>(fso);
	auto existing_version = invalid_version_number;
	if (filish.get_backup_mode() == BackupMode::Full && !this->file_has_changed(existing_version, filish))
		filish.set_backup_mode(BackupMode::Unmodified);
	switch (filish.get_backup_mode()){
		case BackupMode::Unmodified:
			{
				auto temp = make_unique(new UnmodifiedStream);
				temp->set_unique_id(filish.get_stream_id());
				temp->set_containing_version(existing_version);
				temp->set_virtual_size(filish.get_size());
				ret = std::move(temp);
			}
			ret->add_file_system_object(&filish);
			break;
		case BackupMode::ForceFull:
		case BackupMode::Full:
			{
				auto temp = make_unique(new FullStream);
				temp->set_unique_id(filish.get_stream_id());
				temp->set_physical_size(filish.get_size());
				temp->set_virtual_size(filish.get_size());
				ret = std::move(temp);
			}
			ret->add_file_system_object(&filish);
			break;
		case BackupMode::Rsync:
			throw NotImplementedException();
		default:
			throw InvalidSwitchVariableException();
	}
	zekvok_assert(!!ret);
	auto &guid = filish.get_file_system_guid();
	if (guid.valid)
		known_guids[guid.data] = ret;
	return ret;
}

bool compare_hashes(FilishFso &new_file, FilishFso &old_file){
	auto &old_hash = old_file.get_hash();
	if (!old_hash.valid)
		return true;
	if (!new_file.compute_hash())
		return true;
	return new_file.get_hash().digest == old_hash.digest;
}

bool BackupSystem::file_has_changed(version_number_t &dst, FilishFso &new_file){
	dst = invalid_version_number;
	auto path = new_file.get_mapped_path();
	FileSystemObject *old_fso = nullptr;
	for (auto &fso : this->old_objects){
		auto found = fso->find(path);
		if (!found || found->is_directoryish())
			continue;
		old_fso = found;
		break;
	}
	if (!old_fso){
		new_file.compute_hash();
		return true;
	}
	zekvok_assert(old_fso);
	auto old_file = static_cast<FilishFso *>(old_fso);
	auto criterium = this->get_change_criterium(new_file);
	bool ret;
	switch (criterium){
		case ChangeCriterium::ArchiveFlag:
			ret = new_file.get_archive_flag();
			break;
		case ChangeCriterium::Size:
			ret = new_file.get_size() != old_file->get_size();
			break;
		case ChangeCriterium::Date:
			ret = new_file.get_modification_time() != old_file->get_modification_time();
			break;
		case ChangeCriterium::Hash:
			ret = compare_hashes(new_file, *old_file);
			break;
		case ChangeCriterium::HashAuto:
			ret = this->file_has_changed(new_file, *old_file);
			break;
		default:
			throw InvalidSwitchVariableException();
	}
	if (!ret){
		auto &hash = old_file->get_hash();
		if (hash.valid)
			new_file.set_hash(hash.digest);
		auto v = old_file->get_latest_version();
		new_file.set_latest_version(v);
		new_file.set_stream_id(old_file->get_stream_id());
		dst = v;
	}
	return ret;
}

bool BackupSystem::file_has_changed(const FileSystemObject &new_file, const FileSystemObject &old_file){
	return true;
}

ChangeCriterium BackupSystem::get_change_criterium(const FileSystemObject &fso){
	switch (this->change_criterium){
		case ChangeCriterium::ArchiveFlag:
		case ChangeCriterium::Size:
		case ChangeCriterium::Date:
		case ChangeCriterium::Hash:
			return this->change_criterium;
		case ChangeCriterium::HashAuto:
			return fso.get_size() < 1024 * 1024 ? ChangeCriterium::Hash : ChangeCriterium::Date;
	}
	throw InvalidSwitchVariableException();
}

stream_id_t BackupSystem::get_stream_id(){
	return this->next_stream_id++;
}

void BackupSystem::enqueue_file_for_guid_get(FilishFso *fso){
	this->recalculate_file_guids_queue.push_back(fso);
}

std::shared_ptr<VersionForRestore> BackupSystem::compute_latest_version(version_number_t version_number){
	std::shared_ptr<VersionForRestore> latest_version;
	std::map<version_number_t, std::shared_ptr<VersionForRestore>> versions;
	std::vector<version_number_t> stack;
	const auto &latest_version_number = version_number;
	{
		auto version = make_shared(new VersionForRestore(latest_version_number, *this, this->keypair.get()));
		versions[latest_version_number] = version;
		for (auto &object : version->get_base_objects())
			this->old_objects.push_back(object);
		for (auto &dep : version->get_manifest()->version_dependencies)
			versions[dep] = make_shared(new VersionForRestore(dep, *this, this->keypair.get()));
	}
	latest_version = versions[latest_version_number];
	latest_version->fill_dependencies(versions);
	return latest_version;
}

void BackupSystem::restore_backup(version_number_t version_number){
	if (!this->versions.size())
		return;
	if (version_number < 0)
		version_number = this->versions.back() + version_number + 1;
	if (!this->version_exists(version_number))
		throw std::exception("No such version");

	std::cout << "Initializing structures...\n";
	auto latest_version = this->compute_latest_version(version_number);
	std::vector<FileSystemObject *> restore_later;
	for (auto &old_object : this->old_objects){
		old_object->delete_existing();
		for (auto &fso : old_object->get_iterator())
			latest_version->restore(fso, restore_later);
	}
	std::vector<std::pair<version_number_t, FileSystemObject *>> fsos_per_version;
	for (auto &fso : restore_later)
		fsos_per_version.push_back(std::make_pair(fso->get_latest_version(), fso));
	std::sort(
		fsos_per_version.begin(),
		fsos_per_version.end(),
		[](const std::pair<version_number_t, FileSystemObject *> &a, const std::pair<version_number_t, FileSystemObject *> &b){
			if (a.first < b.first)
				return true;
			if (a.first > b.first)
				return false;
			return a.second->get_stream_id() < b.second->get_stream_id();
		}
	);
	restore_later.clear();
	this->perform_restore(latest_version, fsos_per_version);
}

typedef std::vector<std::pair<version_number_t, FileSystemObject *>> restore_vt;

void restore_thread(BackupSystem *This, restore_vt::const_iterator *shared_begin, restore_vt::const_iterator *shared_end, std::mutex *mutex){
	while (true){
		restore_vt::const_iterator begin, end;
		version_number_t version_number;
		{
			std::lock_guard<decltype(*mutex)> guard(*mutex);
			if (*shared_begin == *shared_end)
				break;
			version_number = (*shared_begin)->first;
			begin = *shared_begin;
			end = find_first_true(
				begin,
				*shared_end,
				[version_number](const std::pair<version_number_t, FileSystemObject *> &x){
					return x.second->get_latest_version() > version_number;
				}
			);
			*shared_begin = end;
		}
		//Assume monotonicity of input stream IDs.
		auto begin2 = begin;
		ArchiveReader archive(This->get_version_path(version_number), This->get_keypair().get());
		for (auto &pair : archive.read_everything()){
			auto stream_id = pair.first;
			auto it = begin2;
			FileSystemObject *fso;
			if (it->second->get_stream_id() != stream_id)
				continue;
			fso = it->second;
			++begin2;
			std::wcout << L"Restoring path \"" << fso->get_unmapped_path().wstring() << L"\"\n";
			if (fso->get_type() == FileSystemObjectType::FileHardlink){
				auto hardlink = static_cast<FileHardlinkFso *>(fso);
				hardlink->set_treat_as_file(true);
			}
			fso->restore(*pair.second);
			for (; begin2 != end && begin2->second->get_stream_id() == stream_id; ++begin2){
				std::wcout << L"Hardlink requested. Existing path: \"" << fso->get_mapped_path() << L"\", new path: \"" << begin2->second->get_mapped_path() << "\"\n";
				auto hardlink = static_cast<FileHardlinkFso *>(begin2->second);
				hardlink->set_link_target(fso->get_mapped_path());
				hardlink->restore();
			}
			if (begin2 == end || begin2->second->get_latest_version() > it->second->get_latest_version())
				break;
		}
		zekvok_assert(begin2 == end || begin2->second->get_latest_version() > begin->second->get_latest_version());
	}
}

void BackupSystem::perform_restore(
		const std::shared_ptr<VersionForRestore> &latest_version,
		const restore_vt &restore_later){
	std::vector<std::shared_ptr<std::thread>> threads;
	threads.reserve(std::thread::hardware_concurrency());
	std::mutex mutex;
	auto begin = restore_later.begin();
	auto end = restore_later.end();
	while (threads.size() < threads.capacity())
		threads.push_back(make_shared(new std::thread(restore_thread, this, &begin, &end, &mutex)));
	while (threads.size()){
		threads.back()->join();
		threads.pop_back();
	}
}

std::vector<std::shared_ptr<FileSystemObject>> BackupSystem::get_entries(version_number_t version){
	ArchiveReader archive(this->get_version_path(version), this->keypair.get());
	return archive.get_base_objects();
}

bool BackupSystem::verify(version_number_t version) const{
	if (!this->version_exists(version))
		return false;
	fs::ifstream file(this->get_version_path(version), std::ios::binary);
	if (!file)
		return false;
	file.seekg(0, std::ios::end);
	std::uint64_t size = file.tellg();
	sha256_digest digest;
	file.seekg(-(int)digest.size(), std::ios::end);
	file.read((char *)digest.data(), digest.size());
	if (file.gcount() != digest.size())
		return false;
	file.seekg(0);
	sha256_digest new_digest;
	{
		boost::iostreams::stream<BoundedInputFilter> bounded(file, size - digest.size());
		boost::iostreams::stream<HashInputFilter> hash(bounded, new CryptoPP::SHA256);
		{
			boost::iostreams::stream<NullOutputStream> output(0);
			output << hash.rdbuf();
		}
		hash->get_result(new_digest.data(), new_digest.size());
	}
	return new_digest == digest;
}

bool BackupSystem::full_verify(version_number_t version) const{
	try{
		if (!this->verify(version))
			return false;
		auto deps = ArchiveReader(this->get_version_path(version), this->keypair.get())
			.read_manifest()->version_dependencies;
		for (auto d : deps)
			if (!this->verify(d))
				return false;
		return true;
	}catch (NonFatalException &){
		return false;
	}
}

template <typename T>
std::vector<byte> to_vector(const T &key){
	std::string temp;
	key.Save(CryptoPP::StringSink(temp));
	std::vector<byte> ret;
	ret.resize(temp.size());
	std::copy(temp.begin(), temp.end(), ret.begin());
	return ret;
}

void BackupSystem::generate_keypair(const std::wstring &recipient, const std::wstring &filename, const std::string &symmetric_key){
	while (1){
		CryptoPP::RSA::PrivateKey rsaPrivate;
		rsaPrivate.GenerateRandomWithKeySize(*random_number_generator, 4 << 10);
		if (!rsaPrivate.Validate(*random_number_generator, 3))
			continue;
		CryptoPP::RSA::PublicKey rsaPublic(rsaPrivate);
		if (!rsaPublic.Validate(*random_number_generator, 3))
			continue;

		auto pri = to_vector(rsaPrivate);
		auto pub = to_vector(rsaPublic);

		RsaKeyPair pair(pri, pub, symmetric_key);
		boost::filesystem::ofstream file(filename, std::ios::binary);
		SerializerStream ss(file);
		ss.begin_serialization(pair, false);
		break;
	}
}

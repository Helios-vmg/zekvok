/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "fso.generated.h"
#include "../Exception.h"
#include "../Utility.h"
#include "../BackupSystem.h"
#include "../System/SystemOperations.h"

//------------------------------------------------------------------------------
//CONSTRUCTORS
//------------------------------------------------------------------------------

FileSystemObject::FileSystemObject(const path_t &path, const path_t &unmapped_path, CreationSettings &settings){
	this->backup_system = settings.backup_system;
	this->reporter = settings.reporter;
	this->backup_mode_map.reset(new std::function<BackupMode(const FileSystemObject &)>(settings.backup_mode_map));
	auto container = path.parent_path();
	this->mapped_base_path.reset(new std::wstring(container.wstring()));
	container = unmapped_path.parent_path();
	this->unmapped_base_path.reset(new std::wstring(container.wstring()));
	this->name = unmapped_path.filename().wstring();
	this->set_file_attributes(path);
}

FilishFso::FilishFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): FileSystemObject(path, unmapped_path, settings){
	this->set_members(path);
}

DirectoryishFso::DirectoryishFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): FileSystemObject(path, unmapped_path, settings){
}

DirectoryFso::DirectoryFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): DirectoryishFso(path, unmapped_path, settings){
	this->set_backup_mode();
	if (this->backup_mode == BackupMode::Directory)
		this->children = this->construct_children_list(path);
}

RegularFileFso::RegularFileFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): FilishFso(path, unmapped_path, settings){
	this->set_backup_mode();
}

DirectorySymlinkFso::DirectorySymlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): DirectoryishFso(path, unmapped_path, settings){
	this->set_target(path);
	this->set_backup_mode();
}

JunctionFso::JunctionFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): DirectorySymlinkFso(path, unmapped_path, settings){
}

FileSymlinkFso::FileSymlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): FilishFso(path, unmapped_path, settings){
	this->set_members(path);
	this->set_backup_mode();
}

FileReparsePointFso::FileReparsePointFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): FileSymlinkFso(path, unmapped_path, settings){
	this->set_members(path);
	this->set_backup_mode();
}

FileHardlinkFso::FileHardlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): RegularFileFso(path, unmapped_path, settings){
	this->peers = system_ops::list_all_hardlinks(path.wstring());
	this->set_backup_mode();
}

//------------------------------------------------------------------------------

path_t FileSystemObject::get_mapped_path() const{
	return this->path_override_base();
}

path_t FileSystemObject::get_unmapped_path() const{
	return this->path_override_base(this->get_unmapped_base_path().get(), BasePathType::Unmapped);
}

path_t FileSystemObject::get_path_without_base() const{
	return this->path_override_base(nullptr, BasePathType::Override);
}

path_t FileSystemObject::path_override_base(const std::wstring *base_path_override, BasePathType override) const{
	auto _this = this;
	std::vector<std::wstring> path_vector;
	size_t expected_length = 0;
	while (1){
		auto &name = _this->get_name();
		path_vector.push_back(name);
		expected_length += name.size() + 1;
		if (!_this->get_parent())
			break;
		_this = _this->get_parent();
	}
	const std::wstring *s;
	switch (override){
		case BasePathType::Mapped:
			s = _this->get_mapped_base_path().get();
			break;
		case BasePathType::Unmapped:
			s = _this->get_unmapped_base_path().get();
			break;
		case BasePathType::Override:
			s = base_path_override;
			break;
		default:
			throw InvalidSwitchVariableException();
	}
	if (s){
		path_vector.push_back(*s);
		expected_length += s->size() + 1;
	}

	path_t ret;
	for (auto i = path_vector.rbegin(), e = path_vector.rend(); i != e; ++i)
		ret /= *i;
	return ret;
}

bool pathcmp(const path_t &a, const path_t &b){
	return strcmpci().equal(a.wstring(), b.wstring());
}

bool path_contains_path(path_t dir, path_t file)
{
	dir = dir.normalize();
	file = file.normalize();
	if (dir.filename() == ".")
		dir.remove_filename();
	assert(file.has_filename());
	file.remove_filename();
	auto dir_len = std::distance(dir.begin(), dir.end());
	auto file_len = std::distance(file.begin(), file.end());
	if (dir_len > file_len)
		return false;
	return std::equal(dir.begin(), dir.end(), file.begin(), pathcmp);
}

bool FileSystemObject::contains(const path_t &path) const{
	return path_contains_path(this->get_mapped_path(), path);
}

void FileSystemObject::set_unique_ids(BackupSystem &bs){
	this->stream_id = bs.get_stream_id();
}

void FileSystemObject::set_hash(const sha256_digest &digest){
	this->hash.digest = digest;
	this->hash.valid = true;
}

FileSystemObject *FileSystemObject::create(const path_t &path, const path_t &unmapped_path, CreationSettings &settings){
	switch (system_ops::get_file_system_object_type(path.wstring())){
#define FileSystemObject_create_SWITCH_CASE(x)               \
	case FileSystemObjectType::x:                            \
			return new x##Fso(path, unmapped_path, settings)
		
		FileSystemObject_create_SWITCH_CASE(Directory       );
		FileSystemObject_create_SWITCH_CASE(RegularFile     );
		FileSystemObject_create_SWITCH_CASE(DirectorySymlink);
		FileSystemObject_create_SWITCH_CASE(Junction        );
		FileSystemObject_create_SWITCH_CASE(FileSymlink     );
		FileSystemObject_create_SWITCH_CASE(FileReparsePoint);
		FileSystemObject_create_SWITCH_CASE(FileHardlink    );
	}
	throw InvalidSwitchVariableException();
}

std::shared_ptr<std::istream> FileSystemObject::open_for_exclusive_read(std::uint64_t &size) const{
	auto path = this->get_mapped_path();
	size = system_ops::get_file_size(path.wstring());
	return make_shared(new boost::filesystem::ifstream(path, std::ios::binary));
}

FileSystemObject *FileSystemObject::find(const path_t &_path) const{
	path_t my_base = *this->get_mapped_base_path();
	auto path = _path;
	my_base.normalize();
	path.normalize();
	auto b0 = my_base.begin(),
		e0 = my_base.end();
	auto b1 = path.begin(),
		e1 = path.end();
	for (; b0 != e0 && b1 != e1; ++b0, ++b1)
		if (!strcmpci().equal(b0->wstring(), b1->wstring()))
			return nullptr;
	if (b0 != e0)
		return nullptr;
	if (b1 == e1 || !strcmpci().equal(this->name, b1->wstring()))
		return nullptr;
	b1++;
	if (b1 == e1)
		return nullptr;

}

void FilishFso::set_file_system_guid(const path_t &path, bool retry){
	this->file_system_guid.valid = false;
	try{
		this->file_system_guid.data = system_ops::get_file_guid(path.wstring());
		this->file_system_guid.valid = true;
	}catch (UnableToObtainGuidException &e){
		if (retry)
			this->backup_system->enqueue_file_for_guid_get(this);
		else if (!this->report_error(e, "getting unique ID for \"" + path.string() + "\""))
			throw;
	}catch (NonFatalException &e){
		if (!this->report_error(e, "getting unique ID for \"" + path.string() + "\""))
			throw;
	}
}

void FileSystemObject::set_backup_mode(){
	auto map = this->get_backup_mode_map();
	if (map)
		this->backup_mode = (*map)(*this);
}

std::shared_ptr<std::function<BackupMode(const FileSystemObject &)>> FileSystemObject::get_backup_mode_map(){
	return this->get_root()->backup_mode_map;
}

FileSystemObject *FileSystemObject::get_root(){
	FileSystemObject *_this = this;
	while (_this->parent)
		_this = _this->parent;
	return _this;
}

bool FileSystemObject::report_error(const std::exception &ex, const std::string &context){
	this->add_exception(ex);
	auto r = this->get_reporter();
	return !r || r->report_error(ex, context.c_str());
}

FileSystemObject::ErrorReporter *FileSystemObject::get_reporter(){
	return this->get_root()->reporter.get();
}

bool less_than(const std::shared_ptr<FileSystemObject> &a, const std::shared_ptr<FileSystemObject> &b){
	auto adir = (int)a->is_directoryish(),
		bdir = (int)b->is_directoryish();
	if (adir < bdir)
		return true;
	if (adir > bdir)
		return false;
	return strcmpci()(a->get_name(), b->get_name());
}

std::vector<std::shared_ptr<FileSystemObject>> DirectoryishFso::construct_children_list(const path_t &path){
	boost::filesystem::directory_iterator it(path),
		e;
	std::vector<std::shared_ptr<FileSystemObject>> ret;
	for (; it != e; ++it){
		auto path = it->path();
		ret.push_back(make_shared(this->create_child(path.filename().wstring(), &path)));
	}
	std::sort(ret.begin(), ret.end(), less_than);
	return ret;
}

void DirectorySymlinkFso::set_target(const path_t &path){
	std::wstring s;
	try{
		s = system_ops::get_reparse_point_target(path.wstring());
	}catch (NonFatalException &e){
		if (!this->report_error(e, "getting link target for \"" + path.string() + "\""))
			throw;
	}
	this->link_target.reset(new decltype(s)(s));
}

void FileSymlinkFso::set_members(const path_t &path){
	std::wstring s;
	try{
		s = system_ops::get_reparse_point_target(path.wstring());
	}catch (NonFatalException &e){
		if (!this->report_error(e, "getting link target for \"" + path.string() + "\""))
			throw;
	}
	this->link_target.reset(new decltype(s)(s));
}

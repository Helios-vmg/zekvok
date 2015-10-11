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
#include "../NullStream.h"
#include "../HashFilter.h"
#include "../SymbolicConstants.h"

//------------------------------------------------------------------------------
// default_values()
//------------------------------------------------------------------------------

void FileSystemObject::default_values(){
	this->stream_id = 0;
	this->differential_chain_id = 0;
	this->size = 0;
	this->is_main = false;
	this->parent = nullptr;
	this->latest_version = invalid_version_number;
	this->entry_number = -1;
	this->backup_stream = nullptr;
	this->backup_mode = BackupMode::NoBackup;
	this->archive_flag = false;
	this->backup_system = nullptr;
}

void FileHardlinkFso::default_values(){
	this->treat_as_file = false;
}

//------------------------------------------------------------------------------
// CONSTRUCTORS (A)
//------------------------------------------------------------------------------

FileSystemObject::FileSystemObject(const path_t &path, const path_t &unmapped_path, CreationSettings &settings){
	this->default_values();

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
	throw NotImplementedException();
}

FileHardlinkFso::FileHardlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings): RegularFileFso(path, unmapped_path, settings){
	this->default_values();
	this->set_peers(path.wstring());
	this->set_backup_mode();
}

//------------------------------------------------------------------------------
// CONSTRUCTORS (B)
//------------------------------------------------------------------------------

FileSystemObject::FileSystemObject(FileSystemObject *parent, const std::wstring &name, const path_t *path){
	this->default_values();
	this->parent = parent;
	this->name = name;
	this->set_file_attributes(path ? *path : this->get_mapped_path());
}

DirectoryishFso::DirectoryishFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		FileSystemObject(parent, name, path){
}

DirectoryFso::DirectoryFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		DirectoryishFso(parent, name, path){
	this->set_backup_mode();
	if (this->backup_mode == BackupMode::Directory)
		this->children = this->construct_children_list(path ? *path : this->get_mapped_path());
}

DirectorySymlinkFso::DirectorySymlinkFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		DirectoryishFso(parent, name, path){
	this->set_target(path ? *path : this->get_mapped_path());
	this->set_backup_mode();
}

JunctionFso::JunctionFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		DirectorySymlinkFso(parent, name, path){
}

FilishFso::FilishFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		FileSystemObject(parent, name, path){
	this->set_members(path ? *path : this->get_mapped_path());
}

RegularFileFso::RegularFileFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		FilishFso(parent, name, path){
	this->set_backup_mode();
}

FileHardlinkFso::FileHardlinkFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		RegularFileFso(parent, name, path){
	this->default_values();
	this->set_peers((path ? *path : this->get_mapped_path()).wstring());
	this->set_backup_mode();
}

FileSymlinkFso::FileSymlinkFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		FilishFso(parent, name, path){
	this->set_members(path ? *path : this->get_mapped_path());
	this->set_backup_mode();
}

FileReparsePointFso::FileReparsePointFso(FileSystemObject *parent, const std::wstring &name, const path_t *path):
		FileSymlinkFso(parent, name, path){
	throw NotImplementedException();
}

//------------------------------------------------------------------------------
// get_iterator()
//------------------------------------------------------------------------------

void FileHardlinkFso::set_peers(const std::wstring &path){
	auto result = system_ops::list_all_hardlinks(path);
	if (!result.success)
		throw Win32Exception(result.error);
	this->peers = result.result;
}

void DirectoryFso::iterate(FileSystemObject::iterate_co_t::push_type &sink){
	sink(this);
	for (auto &child : this->children)
		child->iterate(sink);
}

void DirectoryFso::reverse_iterate(FileSystemObject::iterate_co_t::push_type &sink){
	for (auto i = this->children.rbegin(), e = this->children.rend(); i != e; ++i)
		(*i)->reverse_iterate(sink);
	sink(this);
}

void DirectorySymlinkFso::iterate(FileSystemObject::iterate_co_t::push_type &sink){
	sink(this);
}

void DirectorySymlinkFso::reverse_iterate(FileSystemObject::iterate_co_t::push_type &sink){
	this->iterate(sink);
}

void FilishFso::iterate(FileSystemObject::iterate_co_t::push_type &sink){
	sink(this);
}

void FilishFso::reverse_iterate(FileSystemObject::iterate_co_t::push_type &sink){
	this->iterate(sink);
}

FileSystemObject::iterate_co_t::pull_type FileSystemObject::get_iterator(){
	return FileSystemObject::iterate_co_t::pull_type(
		[this](FileSystemObject::iterate_co_t::push_type &sink){
			this->iterate(sink);
		}
	);
}

FileSystemObject::iterate_co_t::pull_type FileSystemObject::get_reverse_iterator(){
	return FileSystemObject::iterate_co_t::pull_type(
		[this](FileSystemObject::iterate_co_t::push_type &sink){
			this->reverse_iterate(sink);
		}
	);
}

//------------------------------------------------------------------------------
// get_type()
//------------------------------------------------------------------------------

#define DEFINE_get_type(x) FileSystemObjectType x##Fso::get_type() const{ return FileSystemObjectType::x; }

DEFINE_get_type(Directory)
DEFINE_get_type(RegularFile)
DEFINE_get_type(DirectorySymlink)
DEFINE_get_type(Junction)
DEFINE_get_type(FileSymlink)
DEFINE_get_type(FileReparsePoint)
DEFINE_get_type(FileHardlink)

//------------------------------------------------------------------------------
// find()
//------------------------------------------------------------------------------

const FileSystemObject *FileSystemObject::find(const path_t &_path) const{
	path_t my_base = *this->get_mapped_base_path();
	my_base.normalize();
	auto path = _path;
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
	return this->find(b1, e1);
}

const FileSystemObject *DirectoryFso::find(path_t::iterator begin, path_t::iterator end) const{
	if (begin == end)
		return nullptr;
	if (!strcmpci().equal(begin->wstring(), this->name))
		return nullptr;
	auto next = begin;
	++next;
	if (next == end)
		return this;
	for (auto &child : this->children){
		auto candidate = child->find(next, end);
		if (candidate)
			return candidate;
	}
	return nullptr;
}

const FileSystemObject *DirectorySymlinkFso::find(path_t::iterator begin, path_t::iterator end) const{
	if (begin == end)
		return nullptr;
	auto next = begin;
	++next;
	if (next != end)
		return nullptr;
	return strcmpci().equal(begin->wstring(), this->name) ? this : nullptr;
}

const FileSystemObject *FilishFso::find(path_t::iterator begin, path_t::iterator end) const{
	if (begin == end)
		return nullptr;
	auto next = begin;
	++next;
	if (next != end)
		return nullptr;
	return strcmpci().equal(begin->wstring(), this->name) ? this : nullptr;
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

void DirectoryFso::set_unique_ids(BackupSystem &bs){
	FileSystemObject::set_unique_ids(bs);
	for (auto &child : this->children)
		child->set_unique_ids(bs);
}

void FilishFso::set_hash(const sha256_digest &digest){
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
	path_t path = path_from_string(this->get_mapped_path().wstring());
	auto result = system_ops::get_file_size(path.wstring());
	if (!result.success)
		throw Win32Exception(result.error);
	size = result.result;
	auto ret = make_shared(new boost::filesystem::ifstream(path, std::ios::binary));
	if (!*ret)
		throw FileNotFoundException(path);
	return ret;
}

void FilishFso::set_file_system_guid(const path_t &path, bool retry){
	auto result = system_ops::get_file_guid(path.wstring());
	this->file_system_guid.valid = result.success;
	if (result.success)
		this->file_system_guid.data = result.result;
	else{
		if (retry){
			auto bss = this->get_backup_system();
			if (bss)
				bss->enqueue_file_for_guid_get(this);
		}else if (!this->report_win32_error(result.error, "getting unique ID for \"" + path.string() + "\""))
			throw Win32Exception(result.error);
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

BackupSystem *FileSystemObject::get_backup_system(){
	return this->get_root()->backup_system;
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

bool FileSystemObject::report_win32_error(std::uint32_t error, const std::string &context){
	std::stringstream stream;
	stream << "Win32 Error: " << error;
	this->exceptions.push_back(stream.str());
	auto r = this->get_reporter();
	return !r || r->report_win32_error(error, context.c_str());
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
	boost::filesystem::directory_iterator it(path_from_string(path.wstring())),
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
	auto result = system_ops::get_reparse_point_target(path.wstring());
	if (!result.success){
		if (!this->report_win32_error(result.error, "getting link target for \"" + path.string() + "\""))
			throw Win32Exception(result.error);
		return;
	}
	auto s = result.result;
	this->link_target.reset(new decltype(s)(s));
}

void FileSymlinkFso::set_members(const path_t &path){
	auto result = system_ops::get_reparse_point_target(path.wstring());
	if (!result.success){
		if (!this->report_win32_error(result.error, "getting link target for \"" + path.string() + "\""))
			throw Win32Exception(result.error);
		return;
	}
	auto s = result.result;
	this->link_target.reset(new decltype(s)(s));
}

void FileSystemObject::set_file_attributes(const path_t &path){
	this->archive_flag = system_ops::get_archive_bit(path.wstring());
	auto error = this->modification_time.set_to_file_modification_time(path.wstring());
	if (error && !this->report_win32_error(error, "getting file system object attributes for \"" + path.string() + "\""))
		throw Win32Exception(error);
}

void FilishFso::set_members(const path_t &path){
	this->size = 0;
	auto result = system_ops::get_file_size(path.wstring());
	if (result.success)
		this->size = result.result;
	else if (!this->report_win32_error(result.error, "getting file size for \"" + path.string() + "\""))
		throw Win32Exception(result.error);
	this->set_file_system_guid(path);
}

void FileSystemObject::add_exception(const std::exception &e){
	this->exceptions.push_back(e.what());
}

FileSystemObject *DirectoryishFso::create_child(const std::wstring &name, const path_t *_path){
	path_t path;
	if (_path)
		path = *_path;
	else{
		path = *this->get_mapped_base_path();
		path /= name;
	}
	switch (system_ops::get_file_system_object_type(path.wstring())){
#define DirectoryishFso_create_child_SWITCH_CASE(x) case FileSystemObjectType::x: return new x##Fso(this, name, &path)
		DirectoryishFso_create_child_SWITCH_CASE(Directory);
		DirectoryishFso_create_child_SWITCH_CASE(RegularFile);
		DirectoryishFso_create_child_SWITCH_CASE(DirectorySymlink);
		DirectoryishFso_create_child_SWITCH_CASE(Junction);
		DirectoryishFso_create_child_SWITCH_CASE(FileSymlink);
		DirectoryishFso_create_child_SWITCH_CASE(FileReparsePoint);
		DirectoryishFso_create_child_SWITCH_CASE(FileHardlink);
	}
	throw InvalidSwitchVariableException();
}

bool FilishFso::compute_hash(sha256_digest &dst){
	if (!this->compute_hash())
		return false;
	dst = this->hash.digest;
	return true;
}

bool FilishFso::compute_hash(){
	if (this->hash.valid)
		return true;
	boost::filesystem::ifstream file(this->get_mapped_path(), std::ios::binary);
	if (!file)
		return false;
	boost::iostreams::stream<NullOutputStream> null_stream(0);
	{
		boost::iostreams::stream<HashOutputFilter> hash_filter(null_stream, new CryptoPP::SHA256);
		hash_filter << file.rdbuf();
		hash_filter->get_result(this->hash.digest.data(), this->hash.digest.size());
	}
	this->hash.valid = true;
	return true;
}

void FileSystemObject::restore(std::istream &, const path_t *base_path){
	throw IncorrectImplementationException();
}

bool FileSystemObject::restore(const path_t *base_path){
	if (this->get_stream_required())
		return false;
	this->restore_internal(base_path);
	return true;
}

void FileSystemObject::restore_internal(const path_t *base_path){
	throw IncorrectImplementationException();
}

void RegularFileFso::restore(std::istream &stream, const path_t *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	auto long_path = path_from_string(path.wstring());
	boost::filesystem::ofstream file(long_path, std::ios::binary);
	if (!file)
		throw CantOpenOutputFileException(path);
	file << stream.rdbuf();
}

void DirectoryFso::restore_internal(const path_t *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	auto long_path = path_from_string(path.wstring());
	boost::filesystem::create_directory(long_path);
}

void DirectorySymlinkFso::restore_internal(const path_t *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	system_ops::create_directory_symlink(path.wstring(), *this->link_target);
}

void FileSymlinkFso::restore_internal(const path_t *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	system_ops::create_symlink(path.wstring(), *this->link_target);
}

void FileReparsePointFso::restore_internal(const path_t *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	system_ops::create_file_reparse_point(path.wstring(), *this->link_target);
}

void JunctionFso::restore_internal(const path_t *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	system_ops::create_junction(path.wstring(), *this->link_target);
}

void FileSystemObject::delete_existing(const std::wstring *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	auto long_path = path_from_string(path.wstring());
	boost::filesystem::remove_all(long_path);
}

void FilishFso::delete_existing_internal(const std::wstring *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	auto long_path = path_from_string(path.wstring());
	if (!boost::filesystem::exists(long_path))
		return;
	boost::filesystem::remove(long_path);
}

void DirectoryishFso::delete_existing_internal(const std::wstring *base_path){
	auto path = this->path_override_unmapped_base_weak(base_path);
	auto long_path = path_from_string(path.wstring());
	if (!boost::filesystem::exists(long_path))
		return;
	boost::filesystem::remove(long_path);
}

bool FileHardlinkFso::restore(const path_t *base_path){
	if (!this->link_target)
		return false;
	auto path = this->path_override_unmapped_base_weak(base_path);
	system_ops::create_hardlink(path.wstring(), *this->link_target);
	return true;
}

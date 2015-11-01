/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "SystemOperations.h"
#include "../AutoHandle.h"
#include <WinIoCtl.h>

std::wstring path_from_string(const std::wstring &path){
	std::wstring ret = path;
	if (ret.size() >= MAX_PATH - 5 && !(ret[0] == '\\' && ret[1] == '\\' && ret[2] == '?' && ret[3] == '\\'))
		ret = system_path_prefix + ret;
	return ret;
}

namespace system_ops{

void enumerate_volumes_helper(const wchar_t *volume, enumerate_volumes_co_t::push_type &sink){
	std::wstring temp = volume;
	temp.resize(temp.size() - 1);
	auto handle = CreateFileW(temp.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	wchar_t volume_name[MAX_PATH * 2];
	if (handle != INVALID_HANDLE_VALUE){
		if (!GetVolumeInformationByHandleW(handle, volume_name, ARRAYSIZE(volume_name), nullptr, nullptr, nullptr, nullptr, 0))
			volume_name[0] = 0;
		CloseHandle(handle);
	}else
		volume_name[0] = 0;

	VolumeInfo vi(volume, volume_name, (DriveType)GetDriveTypeW(volume));
	sink(vi);
}

void enumerate_volumes_helper(enumerate_volumes_co_t::push_type &sink){
	std::vector<wchar_t> buffer(64);
	HANDLE handle;
	while (true){
		handle = FindFirstVolumeW(&buffer[0], (DWORD)buffer.size());
		if (handle != INVALID_HANDLE_VALUE)
			break;

		auto error = GetLastError();
		if (error == ERROR_NO_MORE_FILES)
			return;
		if (error != ERROR_FILENAME_EXCED_RANGE)
			throw Win32Exception(error);
		buffer.resize(buffer.size() * 2);
	}
	while (true){
		enumerate_volumes_helper(&buffer[0], sink);

		bool done = false;
		while (!FindNextVolumeW(handle, &buffer[0], (DWORD)buffer.size())){
			auto error = GetLastError();
			if (!error || error == ERROR_NO_MORE_FILES){
				done = true;
				break;
			}
			if (error != ERROR_FILENAME_EXCED_RANGE)
				throw Win32Exception(error);
			buffer.resize(buffer.size() * 2);
		}
		if (done)
			break;
	}
	FindVolumeClose(handle);
}

enumerate_volumes_co_t::pull_type enumerate_volumes(){
	return enumerate_volumes_co_t::pull_type(enumerate_volumes_helper);
}

VolumeInfo::VolumeInfo(const std::wstring &vp, const std::wstring &vl, DriveType dt):
		volume_path(vp),
		volume_label(vl),
		drive_type(dt){
	auto co = enumerate_mounted_paths(this->volume_path);
	this->mounted_paths = to_vector<std::wstring>(co);
}

enumerate_mounted_paths_co_t::pull_type enumerate_mounted_paths(std::wstring volume_path){
	return enumerate_mounted_paths_co_t::pull_type(
		[volume_path](enumerate_mounted_paths_co_t::push_type &sink){
			std::vector<wchar_t> buffer(64);
			DWORD length;
			while (true){
				auto success = GetVolumePathNamesForVolumeNameW(volume_path.c_str(), &buffer[0], (DWORD)buffer.size(), &length);
				if (success)
					break;
				auto error = GetLastError();
				if (error == ERROR_MORE_DATA){
					buffer.resize(length);
					continue;
				}
				throw Win32Exception(error);
			}
			std::wstring strings(&buffer[0], length);
			size_t pos = 0;
			size_t end;
			while (pos != (end = strings.find((wchar_t)0, pos))){
				if (end == strings.npos)
					break;
				sink(strings.substr(pos, end - pos));
				pos = end + 1;
			}
		}
	);
}

bool internal_is_reparse_point(const wchar_t *path){
	DWORD attr = GetFileAttributesW(path);
	return (attr & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT;
}

bool is_directory(const wchar_t *path){
	return (GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
}

DWORD hardlink_count(const wchar_t *path){
	AutoHandle handle = CreateFileW(path, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	DWORD ret = 0;
	if (handle.handle == INVALID_HANDLE_VALUE)
		return ret;
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileInformationByHandle(handle.handle, &info))
		ret = info.nNumberOfLinks;
	return ret;
}

typedef struct _REPARSE_DATA_BUFFER {
	ULONG  ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	union {
		struct {
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			ULONG Flags;
			WCHAR PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct {
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			WCHAR PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
			UCHAR  DataBuffer[1];
		} GenericReparseBuffer;
	} DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

DWORD internal_get_reparse_point_target(const wchar_t *path, unsigned long *unrecognized, std::wstring *target_path, bool *is_symlink){
	const auto share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	const auto flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
	AutoHandle h = CreateFileW(path, 0, share_mode, nullptr, OPEN_EXISTING, flags, nullptr);
	if (h.handle == INVALID_HANDLE_VALUE)
		return GetLastError();

	USHORT size = 1 << 14;
	std::vector<char> tempbuf(size);
	while (true){
		REPARSE_DATA_BUFFER *buf = (REPARSE_DATA_BUFFER *)&tempbuf[0];
		buf->ReparseDataLength = size;
		DWORD cbOut;
		auto status = DeviceIoControl(h.handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf, size, &cbOut, nullptr);
		auto error = GetLastError();
		if (status){
			switch (buf->ReparseTag){
				case IO_REPARSE_TAG_SYMLINK:
					{
						auto p = (char *)buf->SymbolicLinkReparseBuffer.PathBuffer + buf->SymbolicLinkReparseBuffer.SubstituteNameOffset;
						auto begin = (const wchar_t *)p;
						auto end = (const wchar_t *)(p + buf->SymbolicLinkReparseBuffer.SubstituteNameLength);
						if (!!target_path){
							target_path->assign(begin, end);
							if (starts_with(*target_path, L"\\??\\"))
								*target_path = target_path->substr(4);
						}
						if (is_symlink)
							*is_symlink = true;
					}
					break;
				case IO_REPARSE_TAG_MOUNT_POINT:
					{
						auto p = (char *)buf->MountPointReparseBuffer.PathBuffer + buf->MountPointReparseBuffer.SubstituteNameOffset;
						auto begin = (const wchar_t *)p;
						auto end = (const wchar_t *)(p + buf->MountPointReparseBuffer.SubstituteNameLength);
						if (!!target_path){
							target_path->assign(begin, end);
							if (starts_with(*target_path, L"\\??\\"))
								*target_path = target_path->substr(4);
						}
						if (is_symlink)
							*is_symlink = false;
					}
					break;
				default:
					if (unrecognized)
						*unrecognized = buf->ReparseTag;
					return ERROR_UNIDENTIFIED_ERROR;
			}
			return ERROR_SUCCESS;
		}else{
			if (error == ERROR_MORE_DATA){
				size *= 2;
				tempbuf.resize(size);
				continue;
			}
			return error;
		}
		break;
	}
	return ERROR_UNIDENTIFIED_ERROR;
}

FileSystemObjectType get_file_system_object_type(const std::wstring &_path){
	auto path = path_from_string(_path);

	bool is_symlink = false;
	bool is_rp = internal_is_reparse_point(path.c_str());
	if (!is_directory(path.c_str())){
		if (!is_rp)
			return hardlink_count(path.c_str()) < 2 ? FileSystemObjectType::RegularFile : FileSystemObjectType::FileHardlink;
		internal_get_reparse_point_target(path.c_str(), nullptr, nullptr, &is_symlink);
		return is_symlink ? FileSystemObjectType::FileSymlink : FileSystemObjectType::FileReparsePoint;
	}

	if (!is_rp)
		return FileSystemObjectType::Directory;
	std::wstring target;
	internal_get_reparse_point_target(path.c_str(), nullptr, &target, &is_symlink);
	return is_symlink ? FileSystemObjectType::DirectorySymlink : FileSystemObjectType::Junction;
}

complex_result<std::uint64_t, DWORD> get_file_size(const std::wstring &_path){
	auto path = path_from_string(_path);
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
		return GetLastError();
	return (((std::uint64_t)fad.nFileSizeHigh) << 32) | ((std::uint64_t)fad.nFileSizeLow);
}

complex_result<guid_t, DWORD> get_file_guid(const std::wstring &_path){
	auto path = path_from_string(_path);
#define CREATE_FILE(x) CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, (x), nullptr)
	AutoHandle h = CREATE_FILE(0);
	if (h.handle == INVALID_HANDLE_VALUE)
		h.handle = CREATE_FILE(FILE_FLAG_OPEN_REPARSE_POINT);
	if (h.handle == INVALID_HANDLE_VALUE)
		return ERROR_FILE_NOT_FOUND;

	guid_t ret;
	FILE_OBJECTID_BUFFER buf;
	DWORD cbOut;
	DWORD error = ERROR_UNIDENTIFIED_ERROR;
	static const DWORD rounds[] = {
		FSCTL_CREATE_OR_GET_OBJECT_ID,
		FSCTL_GET_OBJECT_ID,
	};
	for (auto ctl : rounds){
		if (DeviceIoControl(h.handle, ctl, nullptr, 0, &buf, sizeof(buf), &cbOut, nullptr)){
			CopyMemory(ret.data(), &buf.ObjectId, sizeof(GUID));
			error = ERROR_SUCCESS;
			break;
		}else{
			error = GetLastError();
			if (error == ERROR_WRITE_PROTECT)
				continue;
#ifdef _DEBUG
			if (error != ERROR_FILE_NOT_FOUND)
				std::cerr << "DeviceIoControl() in get_file_guid(): " << error << std::endl;
#endif
			break;
		}
	}
	if (error)
		return error;
	return ret;
}

complex_result<std::wstring, DWORD> get_reparse_point_target(const std::wstring &_path){
	unsigned long unrecognized = 0;
	auto path = path_from_string(_path);
	std::wstring ret;
	auto error = internal_get_reparse_point_target(path.c_str(), &unrecognized, &ret, nullptr);
	if (error)
		return error;
	return ret;
}

complex_result<std::vector<std::wstring>, DWORD> list_all_hardlinks(const std::wstring &_path){
	auto path = path_from_string(_path);
	HANDLE handle;
	const DWORD default_size = 1 << 10;
	std::vector<std::wstring> ret;
	{
		DWORD size = default_size;
		std::vector<wchar_t> buffer(size);
		while (1){
			handle = FindFirstFileNameW(path.c_str(), 0, &size, &buffer[0]);
			if (handle == INVALID_HANDLE_VALUE){
				auto error = GetLastError();
				if (error == ERROR_MORE_DATA){
					buffer.resize(size);
					continue;
				}
				return error;
			}
			break;
		}
		buffer.resize(size);
		buffer.push_back(0);
		ret.push_back(std::wstring(&buffer[0], &buffer[buffer.size() - 1]));
	}
	while (1){
		bool Continue = true;
		DWORD size = default_size;
		std::vector<wchar_t> buffer(size);
		while (1){
			Continue = !!FindNextFileNameW(handle, &size, &buffer[0]);
			if (!Continue){
				auto error = GetLastError();
				if (error == ERROR_MORE_DATA){
					buffer.resize(size);
					continue;
				}
				if (error == ERROR_HANDLE_EOF)
					break;
				return error;
			}
		}
		if (!Continue)
			break;
		buffer.resize(size);
		buffer.push_back(0);
		ret.push_back(std::wstring(&buffer[0], &buffer[buffer.size() - 1]));
	}
	FindClose(handle);
	return ret;
}

bool get_archive_bit(const std::wstring &_path){
	auto path = path_from_string(_path);
	return (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_ARCHIVE) == FILE_ATTRIBUTE_ARCHIVE;
}

DWORD create_symlink(const wchar_t *_link_location, const wchar_t *_target_location, bool directory){
	auto link_location = path_from_string(_link_location);
	auto target_location = path_from_string(_target_location);
	if (!CreateSymbolicLinkW(link_location.c_str(), target_location.c_str(), directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0))
		return GetLastError();
	return ERROR_SUCCESS;
}

DWORD create_symlink(const std::wstring &link_location, const std::wstring &target_location){
	return create_symlink(link_location.c_str(), target_location.c_str(), false);
}

DWORD create_directory_symlink(const std::wstring &link_location, const std::wstring &target_location){
	return create_symlink(link_location.c_str(), target_location.c_str(), true);
}

DWORD create_file_reparse_point(const std::wstring &link_location, const std::wstring &target_location){
	throw NotImplementedException();
}

typedef struct {
	DWORD ReparseTag;
	WORD ReparseDataLength;
	WORD Reserved2;
	WORD Reserved;
	WORD ReparseTargetLength;
	WORD ReparseTargetMaximumLength;
	WORD Reserved1;
	WCHAR ReparseTarget[1];
} REPARSE_MOUNTPOINT_DATA_BUFFER, *PREPARSE_MOUNTPOINT_DATA_BUFFER;

DWORD create_junction(const std::wstring &link_location, const std::wstring &target_location){
	auto path = path_from_string(link_location.c_str());
	AutoHandle h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (h.handle == INVALID_HANDLE_VALUE)
		return GetLastError();

	std::wstring target = L"\\??\\";
	target.append(target_location);

	const auto byte_length = target.size() * sizeof(wchar_t);
	const auto byte_length_z = byte_length + sizeof(wchar_t);

	std::vector<char> buffer(offsetof(REPARSE_MOUNTPOINT_DATA_BUFFER, ReparseTarget) + byte_length_z + 2);
	REPARSE_MOUNTPOINT_DATA_BUFFER *rdb = (REPARSE_MOUNTPOINT_DATA_BUFFER *)&buffer[0];
	rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	rdb->ReparseDataLength = (WORD)(byte_length_z + 10);
	rdb->Reserved = 0;
	rdb->Reserved1 = 0;
	rdb->ReparseTargetLength = (WORD)byte_length;
	rdb->ReparseTargetMaximumLength = rdb->ReparseTargetLength + sizeof(wchar_t);
	memcpy(rdb->ReparseTarget, &target[0], byte_length);
	auto status = DeviceIoControl(h.handle, FSCTL_SET_REPARSE_POINT, &buffer[0], (DWORD)buffer.size(), nullptr, 0, nullptr, nullptr);
	if (!status)
		return GetLastError();
	return ERROR_SUCCESS;
}

DWORD create_hardlink(const std::wstring &_link_location, const std::wstring &_existing_file){
	auto link_location = path_from_string(_link_location);
	auto existing_file = path_from_string(_existing_file);
	if (!CreateHardLinkW(link_location.c_str(), existing_file.c_str(), nullptr))
		return GetLastError();
	return ERROR_SUCCESS;
}

}

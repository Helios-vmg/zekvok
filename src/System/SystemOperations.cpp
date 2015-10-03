/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "SystemOperations.h"
#include "../Exception.h"
#include "../Utility.h"
#include "../SymbolicConstants.h"

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
	this->mounted_paths = to_vector<std::wstring>(enumerate_mounted_paths(this->volume_path));
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

std::wstring path_from_string(const std::wstring &path){
	std::wstring ret = path;
	if (ret.size() >= MAX_PATH - 5 && !(ret[0] == '\\' && ret[1] == '\\' && ret[2] == '?' && ret[3] == '\\'))
		ret = system_path_prefix + ret;
	return ret;
}

bool internal_is_reparse_point(const wchar_t *path){
	DWORD attr = GetFileAttributesW(path);
	return (attr & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT;
}

bool is_directory(const wchar_t *path){
	return (GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
}

struct AutoHandle{
	HANDLE handle;
	AutoHandle(HANDLE handle) : handle(handle){}
	~AutoHandle(){
		if (this->handle && this->handle != INVALID_HANDLE_VALUE)
			CloseHandle(this->handle);
	}
};

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

void internal_get_reparse_point_target(const wchar_t *path, unsigned long *unrecognized, std::wstring *target_path, bool *is_symlink){
	const auto share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	const auto flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
	AutoHandle h = CreateFileW(path, 0, share_mode, nullptr, OPEN_EXISTING, flags, nullptr);
	if (h.handle == INVALID_HANDLE_VALUE)
		throw Win32Exception(GetLastError());

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
					throw Win32Exception(ERROR_UNIDENTIFIED_ERROR);
			}
		}else{
			if (error == ERROR_MORE_DATA){
				size *= 2;
				tempbuf.resize(size);
				continue;
			}
			throw Win32Exception(error);
		}
		break;
	}
}

FileSystemObjectType get_file_system_object_type(const std::wstring &_path){
	auto path = path_from_string(_path);

	bool is_symlink = false;
	bool is_rp = internal_is_reparse_point(path.c_str());
	if (!is_directory(path.c_str())){
		if (!is_rp)
			return hardlink_count(path.c_str()) < 2 ? FileSystemObjectType::RegularFile : FileSystemObjectType::FileHardlink;
		try{
			internal_get_reparse_point_target(path.c_str(), nullptr, nullptr, &is_symlink);
		}catch (Win32Exception &){
		}
		return is_symlink ? FileSystemObjectType::FileSymlink : FileSystemObjectType::FileReparsePoint;
	}

	if (!is_rp)
		return FileSystemObjectType::Directory;
	std::wstring target;
	try{
		internal_get_reparse_point_target(path.c_str(), nullptr, &target, &is_symlink);
	}catch (Win32Exception &){
	}
	return is_symlink ? FileSystemObjectType::DirectorySymlink : FileSystemObjectType::Junction;
}

std::uint64_t get_file_size(const std::wstring &_path){
	auto path = path_from_string(_path);
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
		throw Win32Exception(GetLastError());
	return (((std::uint64_t)fad.nFileSizeHigh) << 32) | ((std::uint64_t)fad.nFileSizeLow);
}

guid_t get_file_guid(const std::wstring &_path){
	auto path = path_from_string(_path);
#define CREATE_FILE(x) CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, (x), nullptr)
	AutoHandle h = CREATE_FILE(0);
	if (h.handle == INVALID_HANDLE_VALUE)
		h.handle = CREATE_FILE(FILE_FLAG_OPEN_REPARSE_POINT);
	if (h.handle == INVALID_HANDLE_VALUE)
		throw FileNotFoundException(_path);

	guid_t ret;
	FILE_OBJECTID_BUFFER buf;
	DWORD cbOut;
	bool errored = true;
	static const DWORD rounds[] = {
		FSCTL_CREATE_OR_GET_OBJECT_ID,
		FSCTL_GET_OBJECT_ID,
	};
	for (auto ctl : rounds){
		if (DeviceIoControl(h.handle, ctl, nullptr, 0, &buf, sizeof(buf), &cbOut, nullptr)){
			CopyMemory(ret.data(), &buf.ObjectId, sizeof(GUID));
			errored = false;
			break;
		}else{
			auto error = GetLastError();
			if (error == ERROR_WRITE_PROTECT)
				continue;
#ifdef _DEBUG
			if (error != ERROR_FILE_NOT_FOUND)
				std::cerr << "DeviceIoControl() in get_file_guid(): " << error << std::endl;
#endif
			break;
		}
	}
	if (errored)
		throw UnableToObtainGuidException(_path);
	return ret;
}

std::wstring get_reparse_point_target(const std::wstring &_path){
	unsigned long unrecognized = 0;
	auto path = path_from_string(_path);
	std::wstring ret;
	try{
		internal_get_reparse_point_target(path.c_str(), &unrecognized, &ret, nullptr);
	}catch (Win32Exception &e){
		if (e.get_error() == ERROR_FILE_NOT_FOUND)
			throw FileNotFoundException(_path);
		else
			throw;
	}
	return ret;
}

}

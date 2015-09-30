/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "SystemOperations.h"
#include "../Exception.h"
#include "../Utility.h"

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

}

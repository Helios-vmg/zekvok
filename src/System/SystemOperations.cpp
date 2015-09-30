/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "..\stdafx.h"
#include "SystemOperations.h"
#include "..\Exception.h"

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

	VolumeInfo vi = { volume, volume_name, (DriveType)GetDriveTypeW(volume) };
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

}

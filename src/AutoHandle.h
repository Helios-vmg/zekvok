/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include <Windows.h>

struct AutoHandle{
	HANDLE handle;
	AutoHandle(HANDLE handle) : handle(handle){}
	~AutoHandle(){
		if (this->handle && this->handle != INVALID_HANDLE_VALUE)
			CloseHandle(this->handle);
	}
};

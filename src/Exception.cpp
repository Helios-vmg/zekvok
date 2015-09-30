/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "Exception.h"

Win32Exception::Win32Exception(DWORD error): error(error){
	std::stringstream stream;
	stream << "Win32 error: " << error;
	this->message = stream.str();
}

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

HresultException::HresultException(const char *context, HRESULT hres){
	std::stringstream stream;
	stream << context << " failed with error 0x"
		<< std::hex << std::setw(8) << std::setfill('0') << hres
		<< " (";
	{
		_com_error error(hres);
		for (auto p = error.ErrorMessage(); *p; p++)
			stream << ((unsigned)*p < 0x80 ? (char)*p : '?');
	}
	stream << ")";
	this->message = stream.str();
}

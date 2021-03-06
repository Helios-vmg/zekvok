/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "Exception.h"
#include <DeserializerStream.h>

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

FileNotFoundException::FileNotFoundException(const path_t &path): path(path){
	this->message = "File not found: ";
	this->message += this->path.string();
}

CantOpenOutputFileException::CantOpenOutputFileException(const path_t &path): path(path){
	this->message = "Can't open output file: ";
	this->message += this->path.string();
}

UnableToObtainGuidException::UnableToObtainGuidException(const path_t &path): path(path){
	this->message = "Unable to obtain file system GUID: ";
	this->message += this->path.string();
}

DeserializationException::DeserializationException(int type){
	this->message = "DeserializationError: ";
	switch ((DeserializerStream::ErrorType)type){
		case DeserializerStream::ErrorType::UnexpectedEndOfFile:
			this->message += "Unexpected end of file.";
			break;
		case DeserializerStream::ErrorType::InconsistentSmartPointers:
			this->message += "Serialized stream uses smart pointers inconsistently.";
			break;
		case DeserializerStream::ErrorType::UnknownObjectId:
			this->message += "Serialized stream contains a reference to an unknown object.";
			break;
		case DeserializerStream::ErrorType::InvalidProgramState:
			this->message += "The program is in an unknown state.";
			break;
		case DeserializerStream::ErrorType::MainObjectNotSerializable:
			this->message += "The root object is not an instance of a Serializable subclass.";
			break;
		case DeserializerStream::ErrorType::AllocateAbstractObject:
			this->message += "The stream contains a concrete object with an abstract class type ID.";
			break;
		default:
			throw InvalidSwitchVariableException();
	}
}

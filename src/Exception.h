/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class NonFatalException : public std::exception{
public:
	virtual ~NonFatalException() = 0 {}
};

class StdStringException : public NonFatalException{
protected:
	std::string message;
public:
	StdStringException(){}
	StdStringException(const char *msg): message(msg){}
	StdStringException(const std::string &msg): message(msg){}
	virtual ~StdStringException(){}
	virtual const char *what() const override{
		return this->message.c_str();
	}
};

class Win32Exception : public StdStringException{
	DWORD error;
public:
	Win32Exception(DWORD error);
	DWORD get_error() const{
		return this->error;
	}
};

class FileNotFoundException : public StdStringException{
	path_t path;
public:
	FileNotFoundException(const path_t &path);
};

class CantOpenOutputFileException : public StdStringException{
	path_t path;
public:
	CantOpenOutputFileException(const path_t &path);
};

class UnableToObtainGuidException : public StdStringException{
	path_t path;
public:
	UnableToObtainGuidException(const path_t &path);
};

class HresultException : public StdStringException{
public:
	HresultException(const char *context, HRESULT hres);
};

class DeserializationException : public StdStringException{
public:
	DeserializationException(int);
};

class FatalException : public std::exception{
public:
	virtual ~FatalException() = 0 {}
};

class IncorrectProgramException : public FatalException{
public:
	virtual ~IncorrectProgramException() = 0 {}
};

class InvalidSwitchVariableException : public IncorrectProgramException{
public:
	const char *what() const override{
		return "switch variable outside permissible range";
	}
};

class NotImplementedException : public FatalException{
public:
	const char *what() const override{
		return "This feature is not yet implemented";
	}
};

class IncorrectImplementationException : public IncorrectProgramException{
public:
	const char *what() const override{
		return "The implementation is incorrect";
	}
};

/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

class StdStringException : public std::exception{
protected:
	std::string message;
public:
	StdStringException(){}
	StdStringException(const char *msg): message(msg){}
	StdStringException(const std::string &msg): message(msg){}
	const char *what() const override{
		return this->message.c_str();
	}
};

class Win32Exception : public StdStringException{
	DWORD error;
public:
	Win32Exception(){}
	Win32Exception(DWORD error);
};

class HresultException : public Win32Exception{
public:
	HresultException(const char *context, HRESULT hres);
};

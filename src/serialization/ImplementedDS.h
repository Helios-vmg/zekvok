/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include <DeserializerStream.h>
#include "../Exception.h"

class ImplementedDeserializerStream : public DeserializerStream{
protected:
	void report_error(ErrorType type, const char *info) override{
		throw DeserializationException((int)type);
	}
public:
	ImplementedDeserializerStream(std::istream &stream): DeserializerStream(stream){}
};

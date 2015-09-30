/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"

class BoundedInputFilter : public InputFilter{
	std::streamsize bytes_read,
		simulated_length;
public:
	BoundedInputFilter(std::istream &stream, std::streamsize length):
		InputFilter(stream),
		bytes_read(0),
		simulated_length(length){}
	std::streamsize read(char *s, std::streamsize n) override;
};

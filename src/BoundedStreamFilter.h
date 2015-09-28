/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"

#if 0
class HashOutputFilter : public OutputFilter, public HashCalculator{
protected:
	std::streamsize internal_write(write_callback_t cb, void *ud, const void *input, std::streamsize length) override;
	bool internal_flush(write_callback_t cb, void *ud) override;
public:
	template <typename T>
	HashOutputFilter(): HashCalculator<T>(){}
};
#endif

class BoundedInputFilter : public InputFilter{
	std::streamsize bytes_read,
		simulated_length;
protected:
	std::streamsize internal_read(read_callback_t cb, void *ud, void *output, std::streamsize length) override;
public:
	BoundedInputFilter(std::streamsize length): bytes_read(0), simulated_length(length){}
};

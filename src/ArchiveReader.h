#pragma once
#include "Archive.h"

typedef std::function<void(boost::iostreams::filtering_istream &)> input_filter_generator_t;

class ArchiveReader{
	std::deque<input_filter_generator_t> filters;
	std::unique_ptr<std::istream> stream;

};


/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class ComposingInputFilter{
public:
	typedef boost::coroutines::asymmetric_coroutine<std::istream *> co_t;
private:
	co_t::pull_type *co;
	std::istream *current_stream;
public:
	typedef char char_type;
	typedef boost::iostreams::source_tag category;
	ComposingInputFilter(co_t::pull_type &);
	std::streamsize read(char *s, std::streamsize n);
};


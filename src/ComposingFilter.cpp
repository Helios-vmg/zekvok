/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"

ComposingInputFilter::ComposingInputFilter(co_t::pull_type &co): co(&co), current_stream(nullptr){}

std::streamsize ComposingInputFilter::read(char *s, std::streamsize n){
	std::streamsize ret = 0;
	bool bad = false;
	while (n){

		if (!this->current_stream){
			this->current_stream = this->co->get();
			if (!this->current_stream){
				bad = true;
				break;
			}
			(*this->co)();
		}
		this->current_stream->read(s, n);
		auto read = this->current_stream->gcount();
		ret += read;
		s += read;
		n -= read;
		if (!this->current_stream->good())
			this->current_stream = nullptr;
	}
	return !ret && bad ? -1 : ret;
}

/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"

template <typename F, typename T>
class ProgressFilter{
	F &report;
	T steps,
		size,
		progress,
		last,
		div;
protected:
	void update(size_t length){
		this->progress += length;
		auto next = this->progress / this->div;
		if (next > this->last){
			this->report(this->progress);
			this->last = next;
		}
	}
public:
	ProgressFilter(F &report, T steps, T size): report(report), steps(steps), size(size), last(0){
		this->div = this->size / this->steps;
	}
	virtual ~ProgressFilter(){}
};

template <typename F, typename T>
class ProgressOutputFilter : public OutputFilter, public ProgressFilter<F, T>{
protected:
	std::streamsize write(write_callback_t cb, void *ud, const void *input, std::streamsize length) override{
		std::streamsize ret = 0;
		while (length){
			auto temp = cb(ud, input, length);
			ret += temp;
			input = (const char *)input + temp;
			length -= temp;
		}
		this->update(ret);
		return ret;
	}
	bool flush(write_callback_t cb, void *ud) override{
		return true;
	}
public:
	ProgressOutputFilter(F &report, T steps, T size): ProgressFilter<F, T>(report, steps, size){}
};

template <typename F, typename T>
class ProgressInputFilter : public InputFilter, public ProgressFilter<F, T>{
protected:
	std::streamsize read(read_callback_t cb, void *ud, void *output, std::streamsize length) override{
		std::streamsize ret = 0;
		bool eof = false;
		while (length){
			auto temp = cb(ud, output, length);
			if (temp < 0){
				eof = true;
				break;
			}
			ret += temp;
			output = (char *)output + temp;
			length -= temp;
		}
		this->update(ret);
		return !ret && eof ? -1 : ret;
	}
public:
	ProgressInputFilter(F &report, T steps, T size): ProgressFilter<F, T>(report, steps, size){}
};

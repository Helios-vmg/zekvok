/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "Filters.h"
#include "StreamAdapter.h"

class DumbPipe{
protected:
	std::function<void()> on_empty_read;
	std::list<std::vector<std::uint8_t>> buffers;
	size_t offset;

public:
	typedef char char_type;
	struct category :
		boost::iostreams::bidirectional_device_tag
	{};
	DumbPipe(std::function<void()> &on_empty_read): on_empty_read(on_empty_read), offset(0){}
	std::streamsize write(const char *s, std::streamsize n){
		auto p = (const std::uint8_t *)s;
		this->buffers.emplace_back(std::vector<std::uint8_t>(p, p + n));
		return n;
	}
	std::streamsize read(char *s, std::streamsize n){
		auto p = (std::uint8_t *)s;
		std::streamsize ret = 0;
		bool bad = false;
		while (n){
			if (!this->buffers.size()){
				this->on_empty_read();
				if (!this->buffers.size()){
					bad = true;
					break;
				}
			}
			auto read_size = std::min<size_t>(this->buffers.front().size() - this->offset, n);
			if (read_size)
				memcpy(s, &(this->buffers.front())[0], read_size);
			this->offset += read_size;
			if (this->offset == this->buffers.front().size())
				this->buffers.pop_front();
			ret += read_size;
			s += read_size;
			n -= read_size;
		}
		return !ret && bad ? -1 : ret;
	}
};

class FourWayStreamAdapter::Impl{
	typedef boost::coroutines::asymmetric_coroutine<void> co_t;
	co_t::pull_type co;
	FourWayStreamAdapter *fwsa;
	std::unique_ptr<std::iostream> pipe_in,
		pipe_out;
	co_t::push_type *sink;
public:
	Impl(std::function<void(std::istream &, std::ostream &)> &f, FourWayStreamAdapter *fwsa):
			co(
				[this, &f, fwsa](co_t::push_type &sink){
					this->sink = &sink;
					f(*this->inside_get_istream(), *this->inside_get_ostream());
				}
			),
			fwsa(fwsa),
			sink(nullptr){
		std::function<void()> on_empty_read_in = [this](){
			this->co();
		};
		this->pipe_in.reset(new boost::iostreams::stream<DumbPipe>(on_empty_read_in));
		std::function<void()> on_empty_read_out = [this](){
			assert(!!this->sink);
			(*this->sink)();
		};
		this->pipe_out.reset(new boost::iostreams::stream<DumbPipe>(on_empty_read_out));
	}
	std::ostream *inside_get_ostream(){
		return this->pipe_out.get();
	}
	std::istream *inside_get_istream(){
		return this->pipe_in.get();
	}
	std::ostream *outside_get_ostream(){
		return this->pipe_in.get();
	}
	std::istream *outside_get_istream(){
		return this->pipe_out.get();
	}
};

FourWayStreamAdapter::FourWayStreamAdapter(std::function<void(std::istream &, std::ostream &)> &f){
	this->pimpl.reset(new Impl(f, this));
}

std::ostream *FourWayStreamAdapter::get_ostream(){
	return this->pimpl->outside_get_ostream();
}

std::istream *FourWayStreamAdapter::get_istream(){
	return this->pimpl->outside_get_istream();
}

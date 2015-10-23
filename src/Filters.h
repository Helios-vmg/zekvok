/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class OutputFilter{
protected:
	std::ostream *stream;
	bool flush_enabled;
	int *copies;

	std::streamsize next_write(const char* s, std::streamsize n){
		this->stream->write(s, n);
		if (this->stream->bad())
			return -1;
		return n;
	}
	virtual bool internal_flush(){
		this->stream->flush();
		return this->stream->good();
	}
public:
	typedef char char_type;
	struct category :
		boost::iostreams::sink_tag,
		boost::iostreams::flushable_tag
	{};
	OutputFilter(std::ostream &stream):
		stream(&stream),
		flush_enabled(false),
		copies(new int(1)){}
	OutputFilter(const OutputFilter &b): stream(b.stream), flush_enabled(b.flush_enabled), copies(b.copies){
		++*this->copies;
	}
	virtual ~OutputFilter(){
		if (!--*this->copies)
			delete this->copies;
	}
	virtual std::streamsize write(const char *s, std::streamsize n) = 0;
	virtual bool flush(){
		if (!this->flush_enabled)
			return true;
		return this->internal_flush();
	}
	void enable_flush(){
		this->flush_enabled = true;
	}
};

class InputFilter{
protected:
	std::istream *stream;

	std::streamsize next_read(char *s, std::streamsize n){
		std::streamsize ret = 0;
		bool bad = false;
		while (n){
			this->stream->read(s, n);
			auto count = this->stream->gcount();
			ret += count;
			s += count;
			n -= count;
			if (this->stream->bad() || this->stream->eof()){
				bad = true;
				break;
			}
		}
		return !ret && bad ? -1 : ret;
	}
public:
	typedef char char_type;
	typedef boost::iostreams::source_tag category;
	InputFilter(std::istream &stream): stream(&stream){}
	virtual ~InputFilter(){}
	virtual std::streamsize read(char *s, std::streamsize n) = 0;
};

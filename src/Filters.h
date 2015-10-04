/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class OutputFilter{
protected:
	std::ostream *stream;

	std::streamsize next_write(const char* s, std::streamsize n){
		this->stream->write(s, n);
		if (this->stream->bad())
			return -1;
		return n;
	}
public:
	typedef char char_type;
	struct category :
		boost::iostreams::sink_tag,
		boost::iostreams::flushable_tag
	{};
	OutputFilter(std::ostream &stream): stream(&stream){}
	virtual ~OutputFilter(){}
	virtual std::streamsize write(const char *s, std::streamsize n) = 0;
	virtual bool flush(){
		this->stream->flush();
		return this->stream->good();
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

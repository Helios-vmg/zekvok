/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class OutputFilter{
	template <typename T>
	struct WriteLambdish{
		T &dst;
		WriteLambdish(T &dst): dst(dst){}
		static std::streamsize callback(void *p, const void *buffer, std::streamsize n){
			return ((Lambdish<T> *)p)->write(buffer, n);
		}
	private:
		std::streamsize write(const void *buffer, std::streamsize n){
			return boost::iostreams::write(this->dst, (const char *)buffer, n);
		}
	};
protected:
	typedef std::streamsize (*write_callback_t)(void *p, const void *buffer, std::streamsize n);
    virtual std::streamsize internal_write(write_callback_t cb, void *ud, const void *input, std::streamsize length) = 0;
    virtual bool internal_flush(write_callback_t cb, void *ud) = 0;
public:
    typedef char char_type;
    struct category :
		boost::iostreams::output_filter_tag,
		boost::iostreams::multichar_tag
    {};

    template <typename Sink>
    std::streamsize write(Sink &dst, const char *s, std::streamsize n)
    {
        WriteLambdish<Sink> l(dst);
		return this->internal_write(l.callback, &l, s, n);
    }

	template <typename Sink>
	bool flush(Sink &dst){
        WriteLambdish<Sink> l(dst);
		return this->internal_flush(l.callback, &l) && boost::iostreams::flush(this->dst);
	}

	virtual ~OutputFilter(){}
};

class InputFilter{
	template <typename T>
	struct ReadLambdish{
		T &src;
		ReadLambdish(T &src): src(src){}
		static std::streamsize callback(void *p, void *buffer, std::streamsize n){
			return ((ReadLambdish<T> *)p)->read(buffer, n);
		}
	private:
		std::streamsize read(void *buffer, std::streamsize n){
			return boost::iostreams::read(this->src, (char *)buffer, n);
		}
	};
protected:
	typedef std::streamsize (*read_callback_t)(void *p, void *buffer, std::streamsize n);
	virtual std::streamsize internal_read(read_callback_t cb, void *ud, void *output, std::streamsize length) = 0;
public:
	typedef char char_type;
    struct category :
        boost::iostreams::input_filter_tag,
        boost::iostreams::multichar_tag
    {};

	template<typename Source>
    std::streamsize read(Source &src, char *s, std::streamsize n)
    {
		ReadLambdish<Source> l(src);
		return this->internal_read(l.callback, &l, s, n);
    }

	virtual ~InputFilter(){}
};

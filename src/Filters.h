/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class OutputFilter{
public:
	typedef std::streamsize (*write_callback_t)(void *p, const void *buffer, std::streamsize n);
    virtual std::streamsize write(write_callback_t cb, void *ud, const void *input, std::streamsize length) = 0;
    virtual bool flush(write_callback_t cb, void *ud) = 0;
};

template <typename T>
class OutputFilterCopyable{
	std::shared_ptr<T> impl;
	
	template <typename T2>
	struct WriteLambdish{
		T2 &dst;
		WriteLambdish(T2 &dst): dst(dst){}
		static std::streamsize callback(void *p, const void *buffer, std::streamsize n){
			return ((WriteLambdish<T2> *)p)->write(buffer, n);
		}
	private:
		std::streamsize write(const void *buffer, std::streamsize n){
			return boost::iostreams::write(this->dst, (const char *)buffer, n);
		}
	};
public:
	OutputFilterCopyable(): impl(new T){}
	OutputFilterCopyable(T *f): impl(f){}
	OutputFilterCopyable(const OutputFilterCopyable<T> &b): impl(b.impl){}

    typedef char char_type;
    struct category :
		boost::iostreams::output_filter_tag,
		boost::iostreams::multichar_tag
    {};

    template <typename Sink>
    std::streamsize write(Sink &dst, const char *s, std::streamsize n)
    {
        WriteLambdish<Sink> l(dst);
		return this->impl->write(l.callback, &l, s, n);
    }

	template <typename Sink>
	bool flush(Sink &dst){
        WriteLambdish<Sink> l(dst);
		return this->impl->flush(l.callback, &l) && boost::iostreams::flush(dst);
	}

};

class InputFilter{
public:
	typedef std::streamsize (*read_callback_t)(void *p, void *buffer, std::streamsize n);
	virtual std::streamsize read(read_callback_t cb, void *ud, void *output, std::streamsize length) = 0;
};

template <typename T>
class InputFilterCopyable{
	std::shared_ptr<T> impl;

	template <typename T2>
	struct ReadLambdish{
		T2 &src;
		ReadLambdish(T2 &src): src(src){}
		static std::streamsize callback(void *p, void *buffer, std::streamsize n){
			return ((ReadLambdish<T2> *)p)->read(buffer, n);
		}
	private:
		std::streamsize read(void *buffer, std::streamsize n){
			return boost::iostreams::read(this->src, (char *)buffer, n);
		}
	};
public:
	InputFilterCopyable(): impl(new T){}
	InputFilterCopyable(T *f): impl(f){}
	InputFilterCopyable(const InputFilterCopyable<T> &b): impl(b.impl){}

	typedef char char_type;
    struct category :
        boost::iostreams::input_filter_tag,
        boost::iostreams::multichar_tag
    {};

	template<typename Source>
    std::streamsize read(Source &src, char *s, std::streamsize n)
    {
		ReadLambdish<Source> l(src);
		return this->impl->read(l.callback, &l, s, n);
    }

	T &operator*(){
		return *this->impl;
	}
	const T &operator*() const{
		return *this->impl;
	}
	T *operator->(){
		return this->impl.get();
	}
	const T *operator->() const{
		return this->impl.get();
	}
};

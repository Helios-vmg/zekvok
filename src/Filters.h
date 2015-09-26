#pragma once

class OutputFilter{
	template <typename T>
	struct WriteLambdish{
		T &dst;
		WriteLambdish(T &dst): dst(dst){}
		static std::streamsize callback(void *p, const void *buffer, std::streamsize n){
			return ((Lambdish<Sink> *)p)->write(buffer, n);
		}
	private:
		std::streamsize write(const void *buffer, std::streamsize n){
			return boost::iostreams::write(this->dst, (const char *)buffer, n);
		}
	};
protected:
	typedef std::streamsize (*write_callback_t)(void *p, const void *buffer, std::streamsize n);
    virtual std::streamsize write(write_callback_t cb, void *ud, const void *input, std::streamsize length) = 0;
    virtual bool flush(write_callback_t cb, void *ud) = 0;
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
		return this->write(l.callback, &l, s, n);
    }

	template <typename Sink>
	bool flush(Sink &dst){
        WriteLambdish<Sink> l(dst);
		return this->flush(l.callback, &l) && boost::iostreams::flush(this->dst);
	}
};

class InputFilter{
protected:
	typedef std::streamsize (*read_callback_t)(void *p, void *buffer, std::streamsize n);
	virtual std::streamsize read(read_callback_t cb, void *ud, void *output, std::streamsize length);
public:
	typedef char char_type;
    struct category :
        boost::iostreams::input_filter_tag,
        boost::iostreams::multichar_tag
    {};

	template<typename Source>
    std::streamsize read(Source &src, char *s, std::streamsize n)
    {
		template <typename T>
		struct Lambdish{
			T &src;
			Lambdish(T &src): src(src){}
			static std::streamsize callback(void *p, void *buffer, std::streamsize n){
				return ((Lambdish<Source> *)p)->read(buffer, n);
			}
		private:
			std::streamsize read(void *buffer, std::streamsize n){
				return boost::iostreams::read(this->dest, (const char *)buffer, n);
			}
		};
		Lambdish<Source> l(src);
		return this->read(l.callback, &l, s, n);
    }
};

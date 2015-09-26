#pragma once

struct strcmpci
{
	template <typename CharType>
	bool operator()(const std::basic_string<CharType> &a, const std::basic_string<CharType> &b) const
	{
		auto ap = &a[0];
		auto bp = &b[0];
		auto an = a.size();
		auto bn = b.size();
		auto n = an < bn ? an : bn;
		for (decltype(an) i = 0; i != n; i++)
		{
			auto ac = tolower(ap[i]);
			auto bc = tolower(bp[i]);
			if (ac < bc)
				return true;
			if (ac > bc)
				return false;
		}
		return an < bn;
	}
	template <typename CharType>
	bool equal(const std::basic_string<CharType> &a, const std::basic_string<CharType> &b) const
	{
		auto an = a.size();
		auto bn = b.size();
		if (an != bn)
			return false;
		auto ap = &a[0];
		auto bp = &b[0];
		for (auto i = an; i--;)
		{
			auto ac = tolower(ap[i]);
			auto bc = tolower(bp[i]);
			if (ac != bc)
				return false;
		}
		return true;
	}
	template <typename CharType>
	static bool equal(const std::basic_string<CharType> &a, const CharType *b)
	{
		auto an = a.size();
		auto bn = 0;
		for (; b[bn] != 0; bn++);
		if (an != bn)
			return false;
		auto ap = &a[0];
		auto bp = &b[0];
		for (auto i = an; i--;)
		{
			auto ac = tolower(ap[i]);
			auto bc = tolower(bp[i]);
			if (ac != bc)
				return false;
		}
		return true;
	}
};

template <typename T>
void zero_struct(T &dst){
	memset(&dst, 0, sizeof(dst));
}

template <typename T, size_t N>
void zero_array(T (&dst)[N]){
	memset(&dst, 0, sizeof(dst));
}

struct quick_buffer{
	std::uint8_t *data;
	quick_buffer(size_t n): data(new std::uint8_t[n]){}
	~quick_buffer(){
		delete[] this->data;
	}
	quick_buffer(const quick_buffer &) = delete;
	quick_buffer(quick_buffer &&) = delete;
};

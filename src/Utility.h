/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

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

template <typename DstT, typename SrcT>
DstT deserialize_fixed_le_int(const SrcT &src){
	static_assert(CHAR_BIT == 8, "Only 8-bit byte platforms supported!");

	const std::uint8_t *p = (const std::uint8_t *)src;
	DstT ret = 0;
	for (auto i = sizeof(ret); i--; ){
		ret <<= 8;
		ret |= p[i];
	}
	return ret;
}

template <typename DstT, typename SrcT>
void deserialize_fixed_le_int(DstT &dst, const SrcT &src){
	dst = deserialize_fixed_le_int<DstT, SrcT>(src);
}

template <typename DstT, typename SrcT>
void serialize_fixed_le_int(DstT *dst, const SrcT &src){
	static_assert(CHAR_BIT == 8, "Only 8-bit byte platforms supported!");

	std::uint8_t *p = (std::uint8_t *)dst;
	std::make_unsigned<SrcT>::type copy = src;
	const auto n = sizeof(src);
	for (auto i = 0; i != n; i++){
		p[i] = copy & 0xFF;
		copy >>= 8;
	}
}

template <typename SrcT>
std::array<std::uint8_t, sizeof(SrcT)> serialize_fixed_le_int(const SrcT &src){
	std::array<std::uint8_t, sizeof(SrcT)> ret;
	serialize_fixed_le_int(ret.data(), src);
	return ret;
}

template <typename T, unsigned N>
struct n_bits_on{
	static const T value = (n_bits_on<T, N - 1>::value << 1) | 1;
};

template <typename T>
struct n_bits_on<T, 0>{
	static const T value = 0;
};

template <typename T>
struct all_bits_on{
	static const T value = n_bits_on<T, sizeof(T) * 8>::value;
};

template <typename T>
std::vector<typename T::key_type> get_map_keys(const T &m){
	std::vector<typename T::key_type> ret;
	for (auto &kv : m)
		ret.push_back(kv.first);
	return ret;
}

template <typename T>
std::basic_string<T> ensure_last_character_is_not_backslash(std::basic_string<T> s){
	while (s.back() == '\\')
		s.pop_back();
	return s;
}

template <typename Dst, typename Src>
std::vector<Dst> to_vector(Src &s){
	std::vector<Dst> ret;
	for (auto &x : s)
		ret.push_back(x);
	return ret;
}

template <typename T>
std::basic_string<T> to_string(const char *s){
	std::basic_string<T> ret;
	ret.reserve(strlen(s));
	for (; *s; s++)
		ret.push_back(s);
	return ret;
}

template <typename T>
std::unique_ptr<T> make_unique(T *p){
	return std::unique_ptr<T>(p);
}

template <typename T, size_t N>
size_t array_size(const T (&)[N]){
	return N;
}

template <typename T>
bool starts_with(const std::basic_string<T> &a, const T *b){
	size_t l = 0;
	for (; b[l]; l++);
	if (a.size() < l)
		return false;
	while (l--)
		if (a[l] != b[l])
			return false;
	return true;
}

template <typename T>
std::basic_string<T> to_lower(const std::basic_string<T> &s){
	std::basic_string<T> ret;
	ret.reserve(s.size());
	for (auto c : s)
		ret.push_back(tolower(c));
	return ret;
}

// Given a range [first, last) and a predicate f such that !f(x) for all
// first <= x < y and f(z) for all y <= z < last, find_all() returns y,
// or last if it does not exist.
template<class It, class F>
It find_all(It begin, It end, F &f){
	if (begin >= end)
		return end;
	if (f(*begin))
		return begin;
	auto diff = end - begin;
	while (diff > 1){
		auto pivot = begin + diff / 2;
		if (!f(*pivot))
			begin = pivot;
		else
			end = pivot;
		diff = end - begin;
	}
	return end;
}

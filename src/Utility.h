/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "SimpleTypes.h"
#include "Exception.h"

struct strcmpci
{
	template <typename CharType>
	static bool less_than(const std::basic_string<CharType> &a, const std::basic_string<CharType> &b)
	{
		auto ap = &a[0];
		auto bp = &b[0];
		auto an = a.size();
		auto bn = b.size();
		auto n = std::min(an, bn);
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
	template <typename LeftCharType, typename RightCharType>
	static bool less_than(const std::basic_string<LeftCharType> &a, const RightCharType *b)
	{
		auto an = a.size();
		decltype(an) bn = 0;
		for (; b[bn] != 0; bn++);
		auto ap = &a[0];
		auto bp = &b[0];
		auto n = std::min(an, bn);
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
	template <typename LeftCharType, typename RightCharType>
	static bool greater_than(const std::basic_string<LeftCharType> &a, const RightCharType *b)
	{
		auto an = a.size();
		decltype(an) bn = 0;
		for (; b[bn] != 0; bn++);
		auto ap = &a[0];
		auto bp = &b[0];
		auto n = std::min(an, bn);
		for (decltype(an) i = 0; i != n; i++)
		{
			auto ac = tolower(ap[i]);
			auto bc = tolower(bp[i]);
			if (ac > bc)
				return true;
			if (ac < bc)
				return false;
		}
		return an > bn;
	}
	template <typename LeftCharType, typename RightCharType>
	static bool geq(const std::basic_string<LeftCharType> &a, const RightCharType *b){
		return !less_than(a, b);
	}
	template <typename CharType>
	bool operator()(const std::basic_string<CharType> &a, const std::basic_string<CharType> &b) const
	{
		return less_than(a, b);
	}
	template <typename CharType>
	static bool equal(const std::basic_string<CharType> &a, const std::basic_string<CharType> &b)
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
	template <typename LeftCharType, typename RightCharType>
	static bool equal(const std::basic_string<LeftCharType> &a, const RightCharType *b)
	{
		auto an = a.size();
		decltype(an) bn = 0;
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
	typename std::make_unsigned<SrcT>::type copy = src;
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
	while (s.size() && s.back() == '\\')
		s.pop_back();
	return s;
}

template <typename Dst, typename Src>
std::vector<Dst> to_vector(Src &s){
	std::vector<Dst> ret;
	for (const auto &x : s)
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
		ret.push_back((T)tolower(c));
	return ret;
}

// Given a range [begin, end) and a T->bool mapping f such that
// map(begin, end, f) gives of any of the following forms:
// * [true, ..., true]
// * [false, ..., false, true, ..., true]
// * [false, ..., false]
// Then find_first_true(begin, end, f) returns the first iterator such that
// f(*iterator), or end if it does not exist.
// In other words, given a sorted range [begin, end),
// the range [begin(begin, end, (>= x)), begin(begin, end, (> x)))
// is equivalent to the subrange of [begin, end) that is equal to x.
template<class It, class F>
It find_first_true(It begin, It end, F &f){
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

template<class It, class F, class G>
bool existence_binary_search(It begin, It end, F &partitioner, G &equality){
	auto found = find_first_true(begin, end, partitioner);
	return found != end && equality(*found);
}

template <typename T>
inline void zekvok_assert(const T &condition){
	if (!condition)
		throw IncorrectImplementationException();
}

template <typename T>
std::shared_ptr<T> easy_clone(const T &src){
	buffer_t mem;
	{
		boost::iostreams::stream<MemorySink> omem(&mem);
		SerializerStream stream(omem);
		stream.full_serialization(src);
	}
	std::shared_ptr<T> cloned;
	{
		boost::iostreams::stream<MemorySource> imem(&mem);
		ImplementedDeserializerStream stream(imem);
		cloned.reset(stream.full_deserialization<T>());
	}
	return cloned;
}

inline void simple_buffer_serialization(std::ostream &stream, const buffer_t &buffer){
	auto size = serialize_fixed_le_int<std::uint64_t>(buffer.size());
	stream.write((const char *)size.data(), size.size());
	stream.write((const char *)&buffer[0], buffer.size());
}

inline bool simple_buffer_deserialization(buffer_t &buffer, std::istream &stream){
	char temp[sizeof(std::uint64_t)];
	stream.read(temp, sizeof(temp));
	if (stream.gcount() != sizeof(temp))
		return false;
	auto size64 = deserialize_fixed_le_int<std::uint64_t>(temp);
	size_t size = (size_t)size64;
	if ((std::uint64_t)size != size64)
		return false;
	buffer.resize(size);
	stream.read((char *)&buffer[0], buffer.size());
	return stream.gcount() == buffer.size();
}

inline std::wstring encrypt_string(const std::wstring &s){
	static_assert(sizeof(wchar_t) == 2, "encrypt_string(const std::wstring &) requires 2-byte wchar_t.");
	CryptoPP::SHA256 hash;
	hash.Update((const byte *)s.c_str(), s.size() * sizeof(wchar_t));
	sha256_digest digest;
	hash.Final(digest.data());
	std::wstring ret;
	ret.resize(digest.size() / sizeof(wchar_t));
	memcpy(&ret[0], digest.data(), digest.size());
	return ret;
}

inline std::wstring encrypt_string_ci(const std::wstring &s){
	return encrypt_string(to_lower(s));
}

template <typename T>
std::basic_string<T> get_extension(const std::basic_string<T> &s){
	auto last = s.rfind('.');
	if (last == s.npos)
		return std::basic_string<T>();
	return to_lower(s.substr(last + 1));
}

bool is_text_extension(const std::wstring &);

template <typename T>
std::shared_ptr<T> make_shared(T *p){
	return std::shared_ptr<T>(p);
}

template <typename T>
std::unique_ptr<T> copy_and_make_unique(T &p){
	return std::unique_ptr<T>(new T(p));
}

template <typename T>
class ScopedIncrement{
	T &data;
public:
	ScopedIncrement(T &data): data(data){
		++this->data;
	}
	ScopedIncrement(const ScopedIncrement<T> &) = delete;
	ScopedIncrement(ScopedIncrement<T> &&) = delete;
	~ScopedIncrement(){
		--this->data;
	}
	void operator=(const ScopedIncrement<T> &) = delete;
};

template <typename T>
class ScopedDecrement{
	T &data;
public:
	ScopedDecrement(T &data): data(data){
		--this->data;
	}
	ScopedDecrement(const ScopedDecrement<T> &) = delete;
	ScopedDecrement(ScopedDecrement<T> &&) = delete;
	~ScopedDecrement(){
		++this->data;
	}
	void operator=(const ScopedDecrement<T> &) = delete;
};

template <typename T>
class ScopedAtomicReversibleSet{
	std::atomic<T> &data;
	T old_value;
	bool cancelled;
	bool cancel_on_exception;
public:
	enum CancellationBehavior{
		NoCancellation,
		CancelOnException,
	};
	ScopedAtomicReversibleSet(std::atomic<T> &data, T new_value, CancellationBehavior behavior):
			data(data),
			cancelled(false),
			cancel_on_exception(behavior == CancelOnException){
		this->old_value = this->data.exchange(new_value);
	}
	ScopedAtomicReversibleSet(const ScopedAtomicReversibleSet<T> &) = delete;
	ScopedAtomicReversibleSet(ScopedAtomicReversibleSet<T> &&) = delete;
	~ScopedAtomicReversibleSet(){
		if (this->cancelled)
			return;
		if (this->cancel_on_exception && std::uncaught_exception())
			return;
		this->data = this->old_value;
	}
	void cancel(){
		this->cancelled = true;
	}
	void operator=(const ScopedAtomicReversibleSet<T> &) = delete;
	T get_old_value() const{
		return this->old_value;
	}
};

template <typename T>
class ScopedAtomicPostSet{
	std::atomic<T> &data;
	T new_value;
	bool cancelled;
public:
	ScopedAtomicPostSet(std::atomic<T> &data, T new_value): data(data), cancelled(false), new_value(new_value){}
	ScopedAtomicPostSet(const ScopedAtomicPostSet<T> &) = delete;
	ScopedAtomicPostSet(ScopedAtomicPostSet<T> &&) = delete;
	~ScopedAtomicPostSet(){
		if (!this->cancelled)
			this->data = this->new_value;
	}
	void cancel(){
		this->cancelled = true;
	}
	void operator=(const ScopedAtomicPostSet<T> &) = delete;
};

template <typename T, typename F>
void remove_duplicates(std::vector<T> &v, const F &equality){
	std::vector<T> temp;
	temp.reserve(v.size());
	for (auto &p : v)
		if (!temp.size() || !equality(p, temp.back()))
			temp.push_back(p);
	v = std::move(temp);
}

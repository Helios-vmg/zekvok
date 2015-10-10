public:
	OpaqueTimestamp(): timestamp(0){}
	template <typename FILETIME>
	OpaqueTimestamp(const FILETIME &ft){
		this->set_to_FILETIME(ft);
	}
	static OpaqueTimestamp utc_now();
	void operator=(std::uint64_t);
	bool operator==(const OpaqueTimestamp &) const;
	bool operator!=(const OpaqueTimestamp &) const;
	std::uint32_t set_to_file_modification_time(const std::wstring &path);
	template <typename FILETIME>
	void set_to_FILETIME(const FILETIME &ft){
		this->timestamp = ft.dwHighDateTime;
		this->timestamp <<= 32;
		this->timestamp |= ft.dwLowDateTime;
	}
	void print(std::ostream &) const;

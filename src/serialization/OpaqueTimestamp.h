public:
	OpaqueTimestamp(): timestamp(0){}
	static OpaqueTimestamp utc_now();
	void operator=(std::uint64_t);
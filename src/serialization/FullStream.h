public:
	FullStream(): virtual_size(0), physical_size(0){}
	DEFINE_INLINE_SETTER_GETTER(virtual_size)
	DEFINE_INLINE_SETTER_GETTER(physical_size)
	void get_dependencies(std::set<version_number_t> &dst) const;

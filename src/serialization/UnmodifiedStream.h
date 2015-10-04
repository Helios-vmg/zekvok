public:
	UnmodifiedStream(): virtual_size(0), containing_version(invalid_version_number){}
	void get_dependencies(std::set<version_number_t> &dst) const;
	DEFINE_INLINE_SETTER(containing_version)
	DEFINE_INLINE_SETTER(virtual_size)

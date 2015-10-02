public:
	UnmodifiedStream(){}
	void get_dependencies(std::set<version_number_t> &dst) const;
	DEFINE_INLINE_SETTER(containing_version)
	DEFINE_INLINE_SETTER(virtual_size)

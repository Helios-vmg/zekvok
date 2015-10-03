public:
	DirectoryFso(){}
	virtual FileSystemObjectType get_type() const;
	iterate_co_t::pull_type get_iterator() override;


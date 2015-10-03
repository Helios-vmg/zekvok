public:
	JunctionFso(){}
	JunctionFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	virtual FileSystemObjectType get_type() const;

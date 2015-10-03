public:
	RegularFileFso(){}
	RegularFileFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	RegularFileFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;

private:
	void set_members(const path_t &);
public:
	FileSymlinkFso(){}
	FileSymlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	FileSymlinkFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;

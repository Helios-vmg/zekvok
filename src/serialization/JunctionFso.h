protected:
	virtual void restore_internal(const path_t *base_path) override;
public:
	JunctionFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	JunctionFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;

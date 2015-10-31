private:
	bool treat_as_file;
	void default_values();
	void set_peers(const std::wstring &);
	virtual void encrypt_internal() override;

public:
	FileHardlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	FileHardlinkFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;
	DEFINE_INLINE_SETTER_GETTER(treat_as_file)
	bool restore(const path_t *base_path = nullptr) override;

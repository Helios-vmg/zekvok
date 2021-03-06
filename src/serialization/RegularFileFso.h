public:
	RegularFileFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	RegularFileFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;
	void restore(zstreams::Source *, const path_t *base_path = nullptr) override;
	bool get_stream_required() const override{
		return true;
	}
	
public:
	FilishFso(){}
	void set_file_system_guid(const path_t &, bool retry = true);
	bool compute_hash(sha256_digest &dst) const override;
	bool compute_hash() const override;

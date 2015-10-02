public:
	DirectoryishFso(){}
	bool compute_hash(sha256_digest &dst) const override{
		return false;
	}
	bool compute_hash() const override{
		return false;
	}

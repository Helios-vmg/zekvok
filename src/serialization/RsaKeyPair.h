private:
	std::vector<std::uint8_t> private_key;
	std::string symmetric_key;
	void encrypt();
	void decrypt();
public:
	RsaKeyPair(const std::vector<std::uint8_t> &private_key, const std::vector<std::uint8_t> &public_key, const std::string &symmetric_key);
	const std::vector<std::uint8_t> &get_private_key();
	const std::vector<std::uint8_t> &get_private_key(const std::string &symmetric_key);
	DEFINE_INLINE_GETTER(public_key);

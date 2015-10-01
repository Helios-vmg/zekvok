public:
	FullStream(){}
	std::uint64_t get_virtual_size() const{
		return this->virtual_size;
	}
	void set_virtual_size(std::uint64_t virtual_size){
		this->virtual_size = virtual_size;
	}
	std::uint64_t get_physical_size() const{
		return this->physical_size;
	}
	void set_physical_size(std::uint64_t physical_size){
		this->physical_size = physical_size;
	}


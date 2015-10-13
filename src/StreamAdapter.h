/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

class FourWayStreamAdapter{
	class Impl;
	std::unique_ptr<Impl> pimpl;
public:
	FourWayStreamAdapter(std::function<void(std::istream &, std::ostream &)> &);
	std::ostream *get_ostream();
	std::istream *get_istream();
};

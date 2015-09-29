/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class KernelTransaction{
	HANDLE tx;
	void commit();
	void rollback();
	void release();
public:
	KernelTransaction();
	KernelTransaction(const KernelTransaction &) = delete;
	~KernelTransaction();
	HANDLE get_handle() const{
		return this->tx;
	}
};

class TransactedFileSink{
	HANDLE handle;
public:
	TransactedFileSink(const KernelTransaction &tx, const wchar_t *path);
	size_t write(const void *buffer, size_t size);
};

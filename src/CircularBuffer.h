/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "SimpleTypes.h"

class CircularBuffer{
	byte_t *buffer;
	size_t m_capacity;
	size_t m_size;
	size_t start;
public:
	CircularBuffer(size_t initial_size);
	CircularBuffer(const CircularBuffer &) = delete;
	~CircularBuffer();
	void operator=(const CircularBuffer &) = delete;
	byte_t pop();
	byte_t push(byte_t ret);
	size_t capacity() const{
		return this->m_capacity;
	}
	size_t size() const{
		return this->m_size;
	}
	template <typename T>
	void process_whole(T &f) const{
		((CircularBuffer *)this)->process_whole(f);
	}
	template <typename T>
	void process_whole(T &f){
		if (this->start + this->m_size <= this->m_capacity)
			f(this->buffer + this->start, this->m_size);
		else{
			f(this->buffer + this->start, this->m_capacity - this->start);
			f(this->buffer, (this->start + this->m_size) % this->m_capacity);
		}
	}
	byte_t *data() const{
		return this->buffer + this->start;
	}
	void reset();
	void realloc(size_t n);
	void push_buffer(byte *buf, size_t size){
		size = std::min(size, this->m_capacity - this->m_size);
		auto start = (this->start + this->m_size) % this->m_capacity;
		if (start + size <= this->m_capacity)
			memcpy(this->buffer + start, buf, size);
		else{
			memcpy(this->buffer + start, buf, this->m_capacity - start);
			memcpy(this->buffer, buf, (start + size) % this->m_capacity);
		}
		this->m_size += size;
	}
	void pop_buffer(CircularBuffer &dst){
		auto initial_size = dst.m_size;
		this->process_whole([&](byte *buf, size_t size){ dst.push_buffer(buf, size); });
		auto n = dst.m_size - initial_size;
		this->start += n;
		this->m_size -= n;
	}
	void trim(size_t n){
		if (n > this->m_size)
			return;
		this->m_size = n;
	}
	void reset_size(){
		this->m_size = 0;
	}
	byte_t operator[](size_t i) const{
		return (*(CircularBuffer *)this)[i];
	}
	byte_t &operator[](size_t i){
		return this->buffer[(i % this->m_size + this->start) % this->m_capacity];
	}
	bool single_piece() const{
		return this->start + this->m_size <= this->m_capacity;
	}
};

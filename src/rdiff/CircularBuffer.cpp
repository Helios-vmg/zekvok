/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "CircularBuffer.h"

CircularBuffer::CircularBuffer(size_t initial_size): buffer(nullptr), m_capacity(0){
	this->realloc(initial_size);
}

CircularBuffer::~CircularBuffer(){
	delete[] this->buffer;
}

void CircularBuffer::realloc(size_t n){
	if (this->m_capacity != n){
		delete[] this->buffer;
		this->buffer = new byte_t[n];
	}
	this->m_capacity = n;
	this->reset();
}

byte_t CircularBuffer::pop(){
	if (!this->m_size)
		return 0;
	auto ret = this->buffer[this->start];
	this->start++;
	this->start %= this->m_capacity;
	this->m_size--;
	return ret;
}

byte_t CircularBuffer::push(byte_t ret){
	if (this->m_size == this->m_capacity)
		return 0;
	this->buffer[(this->start + this->m_size++) % this->m_capacity] = ret;
	return ret;
}

void CircularBuffer::reset(){
	this->m_size = this->m_capacity;
	this->start = 0;
}

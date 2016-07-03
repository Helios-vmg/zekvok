/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "../Utility.h"
#include "Threads.h"

std::unique_ptr<ThreadPool> thread_pool;

PooledThread::PooledThread(ThreadPool *pool){
	this->pool = pool;
	this->thread.reset(new std::thread([this](){ this->thread_func(); }));
	this->cv.notify_all();
}

PooledThread::~PooledThread(){
	this->stop();
	this->thread.reset();
}

void PooledThread::stop(){
	this->state = State::Terminating;
	this->cv.notify_all();
	this->thread->join();
}

void PooledThread::join(){
	std::unique_lock<std::mutex> ul(this->reverse_cv_mutex);
	// TODO: This could deadlock if the thread dies without the state
	// being set and/or the CV being notified. I should fix this at some point.
	while (this->state != State::WaitingForJob && this->state != State::Stopped)
		this->reverse_cv.wait(ul);
}

void PooledThread::run(std::unique_ptr<std::function<void()>> &&f){
	this->job = std::move(f);
	this->state = State::JobReady;
	this->cv.notify_all();
}

bool PooledThread::wait_for_job(){
	std::unique_lock<std::mutex> ul(this->cv_mutex);
	while (true){
		State state = this->state;
		if (state == State::JobReady)
			break;
		if (state == State::Terminating)
			return false;
		this->cv.wait(ul);
	}
	return true;
}

void PooledThread::thread_func2(){
	while (true){
		auto new_value = State::WaitingForJob;
		this->state.exchange(new_value);
		if (new_value == State::Terminating)
			return;

		this->reverse_cv.notify_all();

		if (!this->wait_for_job())
			break;

		auto job = std::move(this->job);

		new_value = State::RunningJob;
		this->state.exchange(new_value);
		if (new_value == State::Terminating)
			return;

		(*job)();
	}
}

void PooledThread::thread_func(){
	try{
		this->thread_func2();
	}catch (...){
		this->state = State::Stopped;
		this->reverse_cv.notify_all();
		throw;
	}
	this->state = State::Stopped;
	this->reverse_cv.notify_all();
}

ThreadPool::ThreadPool(){
	
}

ThreadPool::~ThreadPool(){
	for (auto &t : this->all_threads)
		t->stop();
	this->all_threads.clear();
	this->inactive_threads.clear();
}

ThreadWrapper ThreadPool::allocate_thread(){
	LOCK_MUTEX(this->mutex);
	if (this->inactive_threads.size()){
		auto ret = this->inactive_threads.back();
		this->inactive_threads.pop_back();
		return ret;
	}
	auto t = std::make_shared<PooledThread>(this);
	this->all_threads.push_back(t);
	return t.get();
}

void ThreadPool::release_thread(PooledThread *thread){
	LOCK_MUTEX(this->mutex);
	this->inactive_threads.push_back(thread);
}

ThreadWrapper::~ThreadWrapper(){
	this->reset();
}

void ThreadWrapper::reset(){
	if (this->thread)
		this->thread->pool->release_thread(this->thread);
	this->thread = nullptr;
}

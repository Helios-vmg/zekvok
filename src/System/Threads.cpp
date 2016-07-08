/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "../Utility.h"
#include "Threads.h"

const int wait_time = 1;

void Event::signal(){
	std::lock_guard<std::mutex> lock(this->mutex);
	this->signalled = true;
	this->cv.notify_one();
}

void Event::wait(){
	while (true){
		std::unique_lock<std::mutex> lock(this->mutex);
		if (this->signalled){
			this->signalled = false;
			break;
		}
		this->cv.wait(lock);
	}
}

void Event::wait_for(unsigned ms){
	std::unique_lock<std::mutex> lock(this->mutex);
	if (this->signalled){
		this->signalled = false;
		return;
	}
	this->cv.wait_for(lock, std::chrono::milliseconds(ms));
}

std::unique_ptr<ThreadPool> thread_pool;

PooledThread::PooledThread(ThreadPool *pool){
	this->requested_state = RequestedState::None;
	this->reported_state = ReportedState::WaitingForJob;
	this->pool = pool;
	this->thread.reset(new std::thread([this](){ this->thread_func(); }));
}

PooledThread::~PooledThread(){
	this->stop();
}

void PooledThread::stop(){
	if (!this->thread)
		return;
	this->requested_state = RequestedState::Terminate;
	this->requested_state_cv.signal();
	this->thread->join();
	this->thread.reset();
}

void PooledThread::join(){
	// TODO: This could deadlock if the thread dies without the state
	// being set and/or the CV being notified. I should fix this at some point.
	while (true){
		ReportedState state = this->reported_state;
		if (state == ReportedState::WaitingForJob || state == ReportedState::Stopped)
			break;
		this->reported_state_cv.wait();
	}
}

void PooledThread::run(std::unique_ptr<std::function<void()>> &&f){
	this->job = std::move(f);
	this->requested_state = RequestedState::JobReady;
	this->requested_state_cv.signal();
	while (this->reported_state != ReportedState::RunningJob)
		this->reported_state_cv.wait();
	this->requested_state = RequestedState::None;
	this->requested_state_cv.signal();
}

bool PooledThread::wait_for_request(){
	RequestedState state;
	while ((state = this->requested_state) == RequestedState::None)
		this->requested_state_cv.wait();
	return state != RequestedState::Terminate;
}

void PooledThread::wait_for_none(){
	while (this->requested_state != RequestedState::None)
		this->requested_state_cv.wait();
}

void PooledThread::thread_func2(){
	while (true){
		this->reported_state = ReportedState::WaitingForJob;
		this->reported_state_cv.signal();

		if (!this->wait_for_request())
			break;

		auto job = std::move(this->job);

		this->reported_state = ReportedState::RunningJob;
		this->reported_state_cv.signal();

		this->wait_for_none();

		(*job)();
	}
}

void PooledThread::thread_func(){
	try{
		this->thread_func2();
	}catch (...){
		this->reported_state = ReportedState::Stopped;
		this->reported_state_cv.signal();
		throw;
	}
	this->reported_state = ReportedState::Stopped;
	this->reported_state_cv.signal();
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

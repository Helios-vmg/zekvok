/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "Threads.h"

class ThreadExitException : public std::exception{
};

class FiberExitException{
};

ThreadJob *ThreadPool::pop_queue(){
	ThreadJob *ret;
	while (true){
		if (this->stop_signal)
			throw ThreadExitException();
		if (this->job_queue.test_pop_and_requeue(ret, [](ThreadJob *x){ return x->ready(); }))
			return ret;
		this->no_jobs_cv.notify_all();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void ThreadPool::push_queue(ThreadJob *j){
	this->job_queue.push(j);
}

ThreadPool::ThreadPool():
		threads(std::thread::hardware_concurrency()),
		state(ThreadPoolState::Uninitialized){
}

ThreadPool::~ThreadPool(){
	this->stop();
}

template <typename T>
struct ScopedIncrement{
	T &data;
	ScopedIncrement(T &data): data(data){
		++this->data;
	}
	~ScopedIncrement(){
		--this->data;
	}
};

void ThreadPool::switch_to_job(ThreadJob *j){
	ScopedIncrement<decltype(this->running_jobs)> inc(this->running_jobs);
	j->do_work();
}

void ThreadPool::thread_func(){
	try{
		while (true){
			auto j = this->pop_queue();
			this->switch_to_job(j);
			if (!j->is_done()){
				if (!this->scheduling_disabled)
					this->push_queue(j);
			}else
				this->release_job(j);
			this->slice_finished_cv.notify_all();
		}
	}catch (ThreadExitException &){
	}catch (std::exception &){
	}
}

void ThreadPool::release_job(ThreadJob *j){
	LOCK_MUTEX(this->owned_jobs_mutex);
	for (size_t i = 0; i < this->owned_jobs.size(); i++){
		if (this->owned_jobs[i].get() == j){
			this->owned_jobs.erase(this->owned_jobs.begin() + i);
			return;
		}
	}
	zekvok_assert(false);
}

void ThreadPool::add(const std::shared_ptr<ThreadJob> &j){
	j->set_owner(this);
	switch (this->state){
		case ThreadPoolState::Uninitialized:
		case ThreadPoolState::Stopped:
		case ThreadPoolState::Paused:
			this->owned_jobs.push_back(j);
			break;
		case ThreadPoolState::Running:
			{
				LOCK_MUTEX(this->owned_jobs_mutex);
				this->owned_jobs.push_back(j);
			}
			break;
		default:
			zekvok_assert(false);
	}
}

void ThreadPool::start(){
	switch (this->state){
		case ThreadPoolState::Uninitialized:
			for (auto &t : this->threads)
				t.reset(new std::thread([this](){ this->thread_func(); }));
		case ThreadPoolState::Stopped:
		case ThreadPoolState::Paused:
			this->scheduling_disabled = false;
			this->reschedule();
			this->state = ThreadPoolState::Running;
			break;
		case ThreadPoolState::Running:
			break;
		default:
			zekvok_assert(false);
	}
}

void ThreadPool::pause(){
	switch (this->state){
		case ThreadPoolState::Stopped:
		case ThreadPoolState::Paused:
			break;
		case ThreadPoolState::Running:
			{
				std::unique_lock<std::mutex> lock(this->slice_finished_cv_mutex);
				while (this->running_jobs)
					this->slice_finished_cv.wait(lock);
				this->state = ThreadPoolState::Paused;
			}
			break;
		default:
			zekvok_assert(false);
	}
}

void ThreadPool::sync(){
	switch (this->state){
		case ThreadPoolState::Stopped:
		case ThreadPoolState::Paused:
			break;
		case ThreadPoolState::Running:
			{
				std::unique_lock<std::mutex> lock(this->no_jobs_cv_mutex);
				do
					this->no_jobs_cv.wait(lock);
				while (this->running_jobs);
			}
			this->state = ThreadPoolState::Paused;
			break;
		default:
			zekvok_assert(false);
	}
}

void ThreadPool::stop(){
	switch (this->state){
		case ThreadPoolState::Stopped:
			break;
		case ThreadPoolState::Running:
		case ThreadPoolState::Paused:
			this->stop_signal = true;
			break;
		default:
			zekvok_assert(false);
	}
}

void ThreadPool::reschedule(){
	for (auto &j : this->owned_jobs)
		this->job_queue.push(j.get());
}

void FiberThreadPool::set_fiber_addr(void *fiber){
	this->fiber_addrs[std::this_thread::get_id()] = fiber;
}

void FiberThreadPool::thread_func(){
	auto fiber = ConvertThreadToFiber(nullptr);
	if (!fiber)
		return;
	this->set_fiber_addr(fiber);
	ThreadPool::thread_func();
	ConvertFiberToThread();
}

void *FiberThreadPool::get_fiber_addr() const{
	auto it = this->fiber_addrs.find(std::this_thread::get_id());
	zekvok_assert(it != this->fiber_addrs.end());
	return it->second;
}

FiberJob::FiberJob(){
	this->fiber = CreateFiber(1024*1024, static_fiber_func, this);
}

FiberJob::~FiberJob(){
	this->stop();
	DeleteFiber(this->fiber);
}

void FiberJob::set_owner(FiberThreadPool *pool){
	this->fiber_thread_pool = pool;
	ThreadJob::set_owner(pool);
}

void FiberJob::set_owner(ThreadPool *pool){
	this->fiber_thread_pool = dynamic_cast<FiberThreadPool *>(pool);
	if (!this->fiber_thread_pool)
		throw std::exception("Invalid parameter to set_owner()");
	ThreadJob::set_owner(pool);
}

void FiberJob::do_work(){
	this->resume();
}

void CALLBACK FiberJob::static_fiber_func(void *p){
	auto This = static_cast<FiberJob *>(p);
	try{
		This->fiber_func();
	}catch (FiberExitException &){
	}catch (...){
		This->exception_thrown = true;
	}
}

void FiberJob::resume(){
	SwitchToFiber(this->fiber);
	if (this->exception_thrown)
		throw ExceptionThrownInFiberException();
}

void FiberJob::yield(){
	SwitchToFiber(this->fiber_thread_pool->get_fiber_addr());
	if (this->stop_signal)
		throw FiberExitException();
}

void FiberJob::stop(){
	this->stop_signal = true;
	this->resume();
}

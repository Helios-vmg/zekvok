/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "Threads.h"

class ThreadExitException : public std::exception{
};

typedef std::lock_guard<std::mutex> lock_t;

ThreadJob *ThreadPool::pop_queue(){
	while (true){
		{
			lock_t lock(this->queue_mutex);
			auto n = this->job_queue.size();
			if (n){
				for (auto i = n; i--;){
					auto j = this->job_queue.front();
					this->job_queue.pop_front();
					return j;
				}
				this->no_jobs_cv.notify_all();
			}
		}
		this->wait_on_cv();
	}
}

void ThreadPool::push_queue(ThreadJob *j){
	lock_t lock(this->queue_mutex);
	this->job_queue.push_back(j);
	this->queue_cv.notify_one();
}

void ThreadPool::wait_on_cv(){
	std::unique_lock<std::mutex> lock(this->queue_cv_mutex);
	this->queue_cv.wait(lock);
	if (this->stop_signal)
		throw ThreadExitException();
}

ThreadPool::ThreadPool():
		threads(std::thread::hardware_concurrency()),
		state(State::Stopped){
	for (auto &t : this->threads)
		t.reset(new std::thread([this](){ this->thread_func(); }));
}

ThreadPool::~ThreadPool(){
	this->stop();
}

void ThreadPool::thread_func(){
	try{
		while (true){
			auto j = this->pop_queue();
			++this->running_jobs;
			j->do_work();
			--this->running_jobs;
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
	lock_t lock(this->owned_jobs_mutex);
	for (size_t i = 0; i < this->owned_jobs.size(); i++){
		if (this->owned_jobs[i].get() == j){
			this->owned_jobs.erase(this->owned_jobs.begin() + i);
			return;
		}
	}
	assert(false);
}

void ThreadPool::add(const std::shared_ptr<ThreadJob> &j){
	switch (this->state){
		case State::Stopped:
		case State::Paused:
			this->owned_jobs.push_back(j);
			break;
		case State::Running:
			{
				lock_t lock(this->owned_jobs_mutex);
				this->owned_jobs.push_back(j);
			}
			break;
		default:
			assert(false);
	}
}

void ThreadPool::start(){
	switch (this->state){
		case State::Stopped:
		case State::Paused:
			this->scheduling_disabled = false;
			this->reschedule();
			this->state = State::Running;
			break;
		case State::Running:
			break;
		default:
			assert(false);
	}
}

void ThreadPool::pause(){
	switch (this->state){
		case State::Stopped:
		case State::Paused:
			break;
		case State::Running:
			{
				std::unique_lock<std::mutex> lock(this->slice_finished_cv_mutex);
				while (this->running_jobs)
					this->slice_finished_cv.wait(lock);
				this->state = State::Paused;
			}
			break;
		default:
			assert(false);
	}
}

void ThreadPool::sync(){
	switch (this->state){
		case State::Stopped:
		case State::Paused:
			break;
		case State::Running:
			{
				std::unique_lock<std::mutex> lock(this->no_jobs_cv_mutex);
				while (this->running_jobs)
					this->no_jobs_cv.wait(lock);
			}
			this->state = State::Paused;
			break;
		default:
			assert(false);
	}
}

void ThreadPool::stop(){
	switch (this->state){
		case State::Stopped:
			break;
		case State::Running:
		case State::Paused:
			this->stop_signal = true;
			this->queue_cv.notify_all();
			break;
		default:
			assert(false);
	}
}

void ThreadPool::reschedule(){
	for (auto &j : this->owned_jobs)
		this->job_queue.push_back(j.get());
}

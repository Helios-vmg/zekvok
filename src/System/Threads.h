/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class ThreadJob{
	bool done;
public:
	virtual ~ThreadJob(){}
	bool is_done(){
		return this->done;
	}
	virtual void do_work() = 0;
	virtual bool ready() = 0;
};

class ThreadPool{
	std::vector<std::shared_ptr<std::thread>> threads;
	std::vector<std::shared_ptr<ThreadJob>> owned_jobs;
	std::deque<ThreadJob *> job_queue;
	std::mutex owned_jobs_mutex,
		queue_mutex,
		queue_cv_mutex,
		slice_finished_cv_mutex,
		no_jobs_cv_mutex;
	std::condition_variable queue_cv,
		slice_finished_cv,
		no_jobs_cv;
	std::atomic<int> running_jobs;
	std::atomic<bool> stop_signal,
		scheduling_disabled;
	enum class State{
		Stopped,
		Running,
		Paused,
	};
	State state;

	ThreadJob *pop_queue();
	void push_queue(ThreadJob *);
	void wait_on_cv();
	void thread_func();
	void release_job(ThreadJob *);
	void reschedule();
public:
	ThreadPool();
	~ThreadPool();
	void add(const std::shared_ptr<ThreadJob> &);
	void start();
	void pause();
	void sync();
	void stop();
};

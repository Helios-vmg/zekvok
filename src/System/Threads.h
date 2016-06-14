/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class ThreadPool;

class ThreadJob{
protected:
	ThreadPool *owner;
	bool done;
public:
	virtual ~ThreadJob(){}
	bool is_done(){
		return this->done;
	}
	virtual void do_work() = 0;
	virtual bool ready() = 0;
	virtual void set_owner(ThreadPool *pool){
		this->owner = pool;
	}
};

enum class ThreadPoolState{
	Stopped,
	Running,
	Paused,
};

class ThreadPool{
protected:
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
	ThreadPoolState state;

	ThreadJob *pop_queue();
	void push_queue(ThreadJob *);
	void wait_on_cv();
	virtual void thread_func();
	void release_job(ThreadJob *);
	void reschedule();
	virtual void switch_to_job(ThreadJob *);
public:
	ThreadPool();
	virtual ~ThreadPool();
	void add(const std::shared_ptr<ThreadJob> &);
	void start();
	void pause();
	void sync();
	void stop();
};

class FiberThreadPool : public ThreadPool{
protected:
	void *fiber_addr;

	void thread_func() override;
public:
	void *get_fiber_addr() const{
		return this->fiber_addr;
	}
};

class ExceptionThrownInFiberException : public std::exception{
public:
	const char *what() noexcept{
		return "Exception thrown in fiber.";
	}
};

class FiberJob : ThreadJob{
protected:
	void *fiber;
	FiberThreadPool *fiber_thread_pool;
	bool exception_thrown = false;
	bool stop_signal = false;

	static void CALLBACK static_fiber_func(void *This);
	virtual void fiber_func() = 0;
	void resume();
	void yield();
	void stop();
public:
	FiberJob();
	virtual ~FiberJob() = 0;
	void do_work() override;
	virtual void set_owner(ThreadPool *pool) override;
};

/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#define LOCK_MUTEX(x) std::lock_guard<decltype(x)> _CONCAT(LOCK_MUTEX_lock, __COUNTER__)(x)

class QueueBeingDestructed : public std::exception{
public:
	const char *what() const{
		return "An attempt to pop a thread-safe queue could not be completed because the queue is being destructed.";
	}
};

template <typename T>
class ItcQueue{
	std::deque<T> queue;
	std::mutex mutex,
		cv_mutex;
	std::condition_variable cv;
	std::atomic<bool> releasing = false;
	const size_t max_size;
public:
	ItcQueue(size_t max_size = std::numeric_limits<size_t>::max()): max_size(max_size){}
	void enter_release_mode(){
		this->releasing = true;
	}
	bool empty(){
		LOCK_MUTEX(this->mutex);
		return !this->queue.size();
	}
	bool full(){
		LOCK_MUTEX(this->mutex);
		return this->queue.size() >= this->max_size;
	}
	void push(const T &i){
		if (!this->releasing){
			LOCK_MUTEX(this->mutex);
			this->queue.push_back(i);
		}
		this->cv.notify_one();
	}
	T pop(){
		while (true){
			T ret;
			if (this->pop_single_try(ret))
				return ret;

			if (this->releasing)
				throw QueueBeingDestructed();
			std::unique_lock<std::mutex> lock(this->cv_mutex);
			this->cv.wait(lock);
		}
	}
	void put_back(const T &i){
		LOCK_MUTEX(this->mutex);
		this->queue.push_front(i);
	}
	bool pop_single_try(T &ret){
		LOCK_MUTEX(this->mutex);
		if (!this->queue.size())
			return false;
		ret = this->queue.front();
		this->queue.pop_front();
		return true;
	}
	std::unique_lock<decltype(mutex)> acquire_lock(){
		std::unique_lock<decltype(mutex)> ret(this->mutex);
		ret.lock();
		return ret;
	}

	// f is a predicate over T.
	// Returns false if no element in the queue matches the predicate.
	// If the return value is true, dst is set to the first element that matches the predicate.
	// The elements that come before that one are popped and pushed back at the end of the queue.
	//
	// ret = queue.test_pop_and_requeue(dst, f)
	// !ret iff (for all x in queue, !f(x))
	// if ret then let queue@before = part_a ++ k ++ part_b, such that (for all x in part_a, !f(x)),
	// and f(k), then dst = k and queue@after = part_b ++ part_a
	template <typename F>
	bool test_pop_and_requeue(T &dst, const F &f){
		LOCK_MUTEX(this->mutex);
		auto n = this->queue.size();
		for (auto i = n; i--;){
			auto x = this->queue.front();
			this->queue.pop_front();
			if (f(x)){
				dst = x;
				return true;
			}
			this->queue.push_back(x);
		}
		return false;
	}

	void append(ItcQueue<T> &queue){
		LOCK_MUTEX(this->mutex);
		LOCK_MUTEX(queue.mutex);
		for (auto &i : queue.queue)
			this->queue.push_back(i);
	}
	void prepend(ItcQueue<T> &queue){
		LOCK_MUTEX(this->mutex);
		LOCK_MUTEX(queue.mutex);
		for (auto i = queue.queue.rbegin(), e = queue.queue.rend(); i != e; ++i)
			this->queue.push_front(*i);
	}
};

template <typename T>
class CircularQueue{
	std::vector<T> data;
	size_t head, tail;
	const size_t m_capacity;
	std::mutex mutex;
	std::mutex pop_notification_mutex,
		push_notification_mutex;
	std::condition_variable pop_notification,
		push_notification;

	size_t size_no_lock() const{
		auto n = this->m_capacity;
		return (this->tail + n - this->head) % n;
	}
public:
	CircularQueue(size_t max_size): data(size), head(0), tail(0), m_capacity(max_size){}
	size_t capacity() const{
		return this->m_capacity;
	}
	size_t size(){
		LOCK_MUTEX(this->mutex);
		return this->size_no_lock();
	}
	bool full(){
		return this->size() == this->capacity();
	}
	bool empty(){
		return !this->size();
	}
	void push(const T &i){
		const auto n = this->capacity();
		while (true){
			{
				LOCK_MUTEX(this->mutex);
				if (this->size_no_lock() < n){
					this->data[this->tail++ % n] = i;
					this->push_notification.notify_all();
					return;
				}
			}
			std::unique_lock<std::mutex> lock(this->pop_notification_mutex);
			this->pop_notification.wait(lock);
		}
	}
	T pop(){
		const auto n = this->capacity();
		while (true){
			{
				LOCK_MUTEX(this->mutex);
				if (!!this->size_no_lock()){
					auto ret = this->data[this->head++];
					this->head %= n;
					this->pop_notification.notify_all();
					return ret;
				}
			}
			std::unique_lock<std::mutex> lock(this->push_notification_mutex);
			this->push_notification.wait(lock);
		}
	}
	bool try_push(const T &i, unsigned timeout_ms = 100){
		const auto n = this->capacity();
		for (int j = 2; j--;){
			{
				LOCK_MUTEX(this->mutex);
				if (this->size_no_lock() < n){
					this->data[this->tail++ % n] = i;
					this->push_notification.notify_all();
					return true;
				}
			}
			std::unique_lock<std::mutex> lock(this->pop_notification_mutex);
			this->pop_notification.wait_for(lock, std::chrono::milliseconds(timeout_ms));
		}
		return false;
	}
	bool try_pop(T &dst, unsigned timeout_ms = 100){
		const auto n = this->capacity();
		for (int j = 2; j--;){
			{
				LOCK_MUTEX(this->mutex);
				if (!!this->size_no_lock()){
					dst = this->data[this->head++];
					this->head %= n;
					this->pop_notification.notify_all();
					return true;
				}
			}
			std::unique_lock<std::mutex> lock(this->push_notification_mutex);
			this->push_notification.wait_for(lock, std::chrono::milliseconds(timeout_ms));
		}
		return false;
	}
};

class ThreadPool;

class ThreadJob{
protected:
	ThreadPool *owner = nullptr;
	bool done = false;
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
	Uninitialized,
	Stopped,
	Running,
	Paused,
};

class ThreadPool{
protected:
	std::vector<std::shared_ptr<std::thread>> threads;
	std::vector<std::shared_ptr<ThreadJob>> owned_jobs;
	ItcQueue<ThreadJob *> job_queue;
	std::mutex owned_jobs_mutex,
		slice_finished_cv_mutex,
		no_jobs_cv_mutex;
	std::condition_variable slice_finished_cv,
		no_jobs_cv;
	std::atomic<int> running_jobs = 0;
	std::atomic<bool> stop_signal = false,
		scheduling_disabled = false;
	ThreadPoolState state;

	ThreadJob *pop_queue();
	void push_queue(ThreadJob *);
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
	std::unordered_map<std::thread::id, void *> fiber_addrs;

	void thread_func() override;
	void set_fiber_addr(void *);
public:
	void *get_fiber_addr() const;
};

class ExceptionThrownInFiberException : public std::exception{
public:
	const char *what() noexcept{
		return "Exception thrown in fiber.";
	}
};

class FiberJob : public ThreadJob{
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
	virtual void set_owner(FiberThreadPool *pool);
};

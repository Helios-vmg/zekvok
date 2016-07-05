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
class CircularQueue{
	std::unique_ptr<T[]> datap;
	T *data;
	size_t head, tail;
	size_t size;
	const size_t capacity;
	std::mutex mutex;
	std::mutex pop_notification_mutex,
		push_notification_mutex;
	std::condition_variable pop_notification,
		push_notification;

public:
	CircularQueue(size_t max_size):
		datap(new T[max_size]),
		data(datap.get()),
		head(0),
		tail(0),
		size(0),
		capacity(max_size){}
	bool try_push(T &i, unsigned timeout_ms = 100){
		const auto n = this->capacity;
		for (int j = 0; ;){
			{
				LOCK_MUTEX(this->mutex);
				if (this->size < n){
					this->data[this->tail++ % n] = std::move(i);
					this->tail %= n;
					this->size++;
					this->push_notification.notify_all();
					return true;
				}
			}
			if (++j == 2)
				break;
			std::unique_lock<std::mutex> lock(this->pop_notification_mutex);
			this->pop_notification.wait_for(lock, std::chrono::milliseconds(timeout_ms));
		}
		return false;
	}
	bool try_pop(T &dst, unsigned timeout_ms = 100){
		const auto n = this->capacity;
		for (int j = 0; ;){
			{
				LOCK_MUTEX(this->mutex);
				if (this->size){
					dst = std::move(this->data[this->head++]);
					this->head %= n;
					this->size--;
					this->pop_notification.notify_all();
					return true;
				}
			}
			if (++j == 2)
				break;
			std::unique_lock<std::mutex> lock(this->push_notification_mutex);
			this->push_notification.wait_for(lock, std::chrono::milliseconds(timeout_ms));
		}
		return false;
	}
};

class ThreadPool;

class PooledThread{
	friend class ThreadWrapper;
	std::shared_ptr<std::thread> thread;
	std::mutex requested_state_cv_mutex, reported_state_cv_mutex;
	std::condition_variable requested_state_cv, reported_state_cv;
	enum class RequestedState{
		None,
		JobReady,
		Terminate,
	};
	enum class ReportedState{
		WaitingForJob,
		RunningJob,
		Stopping,
		Stopped,
	};
	std::atomic<RequestedState> requested_state;
	std::atomic<ReportedState> reported_state;
	std::unique_ptr<std::function<void()>> job;
	ThreadPool *pool = nullptr;

	void thread_func();
	void thread_func2();
	bool wait_for_request();
	void wait_for_none();
public:
	PooledThread(ThreadPool *pool);
	PooledThread(const PooledThread &) = delete;
	PooledThread(PooledThread &&) = delete;
	~PooledThread();
	void set_pool(ThreadPool *pool){
		this->pool = pool;
	}
	void stop();
	void join();
	void run(std::unique_ptr<std::function<void()>> &&);
};

class ThreadWrapper{
	PooledThread *thread = nullptr;
public:
	ThreadWrapper(){}
	ThreadWrapper(PooledThread *thread): thread(thread){}
	ThreadWrapper(ThreadWrapper &&tw): thread(nullptr){
		*this = std::move(tw);
	}
	const ThreadWrapper &operator=(ThreadWrapper &&tw){
		this->thread = tw.thread;
		tw.thread = nullptr;
		return *this;
	}
	const ThreadWrapper &operator=(const ThreadWrapper &) = delete;
	~ThreadWrapper();
	PooledThread &operator*() const{
		return *this->thread;
	}
	PooledThread *operator->() const{
		return this->thread;
	}
	bool operator!() const{
		return !this->thread;
	}
	void reset();
};

class ThreadPool{
	std::vector<std::shared_ptr<PooledThread>> all_threads;
	std::vector<PooledThread *> inactive_threads;
	std::mutex mutex;
public:
	ThreadPool();
	~ThreadPool();
	ThreadWrapper allocate_thread();
	void release_thread(PooledThread *);
};

extern std::unique_ptr<ThreadPool> thread_pool;

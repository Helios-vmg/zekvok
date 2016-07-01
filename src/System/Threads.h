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

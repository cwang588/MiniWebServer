//
// Created by chenghao on 11/18/22.
//

#include "../header/threadpool.h"
#include "../header/connectionpool.h"
template<typename T>
ThreadPool<T>::ThreadPool(int actor_model, ConnectionPool *connection_pool, int max_thread,
                          int max_request):actor_model_(actor_model), connection_pool_(connection_pool),
                                           max_thread_(max_thread), max_request_(max_request) {
    if (max_thread <= 0 || max_request <= 0)throw std::exception();
    threads_ = new pthread_t[max_thread];
    if (threads_ == nullptr)throw std::exception();
    for (int i = 0; i < max_thread; ++i) {
        if (pthread_create(threads_[i], nullptr, Work, this)) {
            delete[] threads_;
            throw std::exception();
        }
        if (pthread_detach(threads_[i])) {
            delete[] threads_;
            throw std::exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] threads_;
}

template<typename T>
bool ThreadPool<T>::Append(T *request, int state) {
    queue_locker_.Lock();
    if (work_queue_.size() >= max_request_) {
        queue_locker_.Unlock();
        return false;
    }
    request->state_ = state;
    work_queue_.push_back(request);
    queue_locker_.Unlock();
    queue_status_.Post();
    return true;
}

template<typename T>
bool ThreadPool<T>::AppendP(T *request) {
    queue_locker_.Lock();
    if (work_queue_.size() >= max_request_) {
        queue_locker_.Unlock();
        return false;
    }
    work_queue_.push_back(request);
    queue_locker_.Unlock();
    queue_status_.Post();
    return true;
}

template<typename T>
void ThreadPool<T>::Run() {
    while (true) {
        queue_status_.Wait();
        queue_locker_.Lock();
        if (work_queue_.empty()) {
            queue_locker_.Unlock();
            continue;
        }
        T *current_request = work_queue_.front();
        work_queue_.pop_front();
        queue_locker_.Unlock();
        if (current_request == nullptr)continue;
        switch (actor_model_) {
            case PROACTOR: {

            }
            case REACTOR: {
                ConnectionRAII mysql_connection(&current_request.state_, connection_pool_);
                current_request->Process();
            }
            default:
                break;
        }

    }
}

template<typename T>
void *ThreadPool<T>::Work(void *arg) {
    ThreadPool *thread_pool = (ThreadPool *) arg;
    thread_pool->Run();
    return thread_pool;
}
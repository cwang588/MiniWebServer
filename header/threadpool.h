//
// Created by chenghao on 11/9/22.
//

#ifndef MINIWEBSERVER_THREADPOOL_H
#define MINIWEBSERVER_THREADPOOL_H

#include<pthread.h>
#include<list>
#include"locker.h"
#include"connectionpool.h"

template<typename T>
class ThreadPool {
public:
    enum ActorModel {
        PROACTOR,
        REACTOR
    };
public:
    ThreadPool(int actor_model, ConnectionPool *connection_pool, int max_thread,
               int max_request);

    ~ThreadPool();

    bool Append(T *request, int state);

    bool AppendP(T *request);

private:
    static void *Work(void *arg);

    void Run();

private:
    int max_thread_;
    int max_request_;
    pthread_t *threads_;
    std::list<T *> work_queue_;
    Locker queue_locker_;
    Semaphore queue_status_;
    ConnectionPool *connection_pool_;
    int actor_model_;
};

#endif //MINIWEBSERVER_THREADPOOL_H

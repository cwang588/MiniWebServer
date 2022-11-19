//
// Created by chenghao on 11/3/22.
//

#ifndef MINIWEBSERVER_LOCKER_H
#define MINIWEBSERVER_LOCKER_H

#include<exception>
#include<semaphore.h>

class Semaphore {
public:
    Semaphore();

    Semaphore(int num);

    ~Semaphore();

    bool Wait();

    bool Post();

private:
    sem_t sem_;
};

class Locker {
public:
    Locker();

    ~Locker();

    bool Lock();

    bool Unlock();

    pthread_mutex_t *Get();

private:
    pthread_mutex_t mutex_;
};

class Conditioner {
public:
    Conditioner();

    ~Conditioner();

    bool Wait(pthread_mutex_t *mutex);

    bool TimeWait(pthread_mutex_t *mutex, timespec t);

    bool Signal();

    bool Broadcast();

private:
    pthread_cond_t cond_;
};

#endif //MINIWEBSERVER_LOCKER_H

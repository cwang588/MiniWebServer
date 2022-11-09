//
// Created by chenghao on 11/9/22.
//
#include "../header/locker.h"
#include <exception>
#include <pthread.h>
#include <semaphore.h>

Semaphore::Semaphore() {
    if(sem_init(&sem_,0,0))throw std::exception();
}

Semaphore::Semaphore(int num) {
    if(sem_init(&sem_,0,num))throw std::exception();
}

Semaphore::~Semaphore() {
    sem_destroy(&sem_);
}

bool Semaphore::Wait() {
    return !sem_wait(&sem_);
}

bool Semaphore::Post() {
    return !sem_post(&sem_);
}

Locker::Locker() {
    if(pthread_mutex_init(&mutex_, nullptr))throw std::exception();
}

Locker::~Locker() {
    pthread_mutex_destroy(&mutex_);
}

bool Locker::Lock() {
    return !pthread_mutex_lock(&mutex_);
}

bool Locker::Unlock() {
    return !pthread_mutex_unlock(&mutex_);
}

pthread_mutex_t *Locker::Get() {
    return &mutex_;
}

Conditioner::Conditioner() {
    if(pthread_cond_init(&cond_, nullptr))throw std::exception();
}

Conditioner::~Conditioner() {
    pthread_cond_destroy(&cond_);
}

bool Conditioner::Wait(pthread_mutex_t *mutex) {
    return !pthread_cond_wait(&cond_,mutex);
}

bool Conditioner::TimeWait(pthread_mutex_t *mutex, timespec t) {
    return !pthread_cond_timedwait(&cond_,mutex,&t);
}

bool Conditioner::Signal(){
    return !pthread_cond_signal(&cond_);
}

bool Conditioner::Broadcast() {
    return !pthread_cond_broadcast(&cond_);
}
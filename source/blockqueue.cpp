//
// Created by chenghao on 11/22/22.
//
#include "../header/blockqueue.h"

template<typename T>
BlockQueue<T>::BlockQueue(int max_size) {
    if (max_size < 0)exit(-1);
    max_size_ = max_size;
    array_ = new T[max_size_];
    size_ = 0;
    front_ = back_ = -1;
}

template<typename T>
BlockQueue<T>::~BlockQueue() {
    locker_.Lock();
    if (array_ != nullptr)delete[] array_;
    locker_.Unlock();
}

template<typename T>
void BlockQueue<T>::Clear() {
    locker_.Lock();
    size_ = 0;
    front_ = back_ = -1;
}

template<typename T>
bool BlockQueue<T>::Full() {
    locker_.Lock();
    bool re = size_ == max_size_;
    locker_.Unlock();
    return re;
}

template<typename T>
bool BlockQueue<T>::Empty() {
    locker_.Lock();
    bool re = size_ == 0;
    locker_.Unlock();
    return re;
}

template<typename T>
bool BlockQueue<T>::Front(T &value) {
    locker_.Lock();
    bool re = size_ != 0;
    if (re)value = array_[front_];
    locker_.Unlock();
    return re;
}

template<typename T>
bool BlockQueue<T>::Back(T &value) {
    locker_.Lock();
    bool re = size_ != 0;
    if (re)value = array_[back_];
    locker_.Unlock();
    return re;
}

template<typename T>
int BlockQueue<T>::Size() {
    int re;
    locker_.Lock();
    re = size_;
    locker_.Unlock();
    return re;
}

template<typename T>
int BlockQueue<T>::MaxSize() {
    int re;
    locker_.Lock();
    re = max_size_;
    locker_.Unlock();
    return re;
}

template<typename T>
bool BlockQueue<T>::Push(T &value) {
    locker_.Lock();
    if (size_ >= max_size_) {
        conditioner_.Broadcast();
        locker_.Unlock();
        return false;
    }
    back_ = (back_ + 1) % max_size_;
    array_[back_] = value;
    ++size_;
    conditioner_.Broadcast();
    locker_.Unlock();
    return true;
}

template<typename T>
bool BlockQueue<T>::Pop(T &value) {
    locker_.Unlock();
    while (!size_) {
        if (!conditioner_.Wait(locker_.Get())) {
            locker_.Unlock();
            return false;
        }
    }
    --size_;
    front_ = (front_ + 1) % max_size_;
    value = array_[front_];
    locker_.Unlock();
    return true;
}

template<typename T>
bool BlockQueue<T>::Pop(T &value, int timeout_ms) {
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    locker_.Lock();
    if (size_ <= 0) {
        t.tv_sec = now.tv_sec + timeout_ms / 1000;
        t.tv_nsec = (timeout_ms % 1000) * 1000;
        if (!conditioner_.TimeWait(locker_.Get(), t)) {
            locker_.Unlock();
            return false;
        }
    }
    if (size_ <= 0) {
        locker_.Unlock();
        return false;
    }
    front_ = (front_ + 1) % max_size_;
    value = array_[front_];
    locker_.Unlock();
    return true;
}

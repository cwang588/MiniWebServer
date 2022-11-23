//
// Created by chenghao on 11/22/22.
//

#ifndef MINIWEBSERVER_BLOCKQUEUE_H
#define MINIWEBSERVER_BLOCKQUEUE_H

#include <cstdlib>
#include <sys/time.h>
#include "locker.h"


template<typename T>
class BlockQueue {
public:
    explicit BlockQueue(int max_size = 1000);

    ~BlockQueue();

    void Clear();

    bool Full();

    bool Empty();

    bool Front(T &value);

    bool Back(T &value);

    int Size();

    int MaxSize();

    bool Push(T &value);

    bool Pop(T &value);

    bool Pop(T &value, int timeout_ms);

private:
    Locker locker_;
    Conditioner conditioner_;
    T *array_;
    int size_, max_size_, front_, back_;
};

#endif //MINIWEBSERVER_BLOCKQUEUE_H

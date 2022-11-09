//
// Created by chenghao on 11/9/22.
//

#ifndef MINIWEBSERVER_THREADPOOL_H
#define MINIWEBSERVER_THREADPOOL_H

template<typename T>
class ThreadPool {
public:
    ThreadPool(int actor_model, con);
    ~ThreadPool();
    bool Append();
    bool AppendP();
};

#endif //MINIWEBSERVER_THREADPOOL_H

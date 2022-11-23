//
// Created by chenghao on 11/22/22.
//

#ifndef MINIWEBSERVER_TIMER_H
#define MINIWEBSERVER_TIMER_H

#include <unistd.h>
#include <csignal>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctime>
#include <fcntl.h>
#include <sys/epoll.h>
#include "http.h"

class UtilTimer;

struct ClientData {
    sockaddr_in address_;
    int socket_fd_;
    UtilTimer *timer;
};

class UtilTimer {
public:
    UtilTimer() : prev_(nullptr), next_(nullptr) {}

public:
    void (*cb_func)(ClientData *);

    UtilTimer *prev_, *next_;
    ClientData *user_data;
    time_t expire_;
};

class TimerList {
public:
    TimerList() : head_(nullptr), tail_(nullptr){}

    ~TimerList();

    void AddTimer(UtilTimer *timer);

    void AdjustTimer(UtilTimer *timer);

    void DelTimer(UtilTimer *timer);

    void Tick();

private:
    void AddTimer(UtilTimer *timer, UtilTimer *list_head);

private:
    UtilTimer *head_, *tail_;
};

class Utils {
public:
    Utils() {}

    ~Utils() {}

    void Init(int timeslot);

    int SetNonBlocking(int fd);

    void AddFd(int epoll_fd, int fd, bool one_shot, int trig_mode);

    static void SignalHandler(int signal);

    void AddSignal(int signal, void(*handler)(int), bool restart = true);

    void TimerHandler();

    void ShowError(int connection_fd, const char *info);

public:
    static int *pipe_fd_;
    TimerList timer_list_;
    static int epoll_fd_;
    int time_slot_;
};

void CbFunc(ClientData *user_data);

#endif //MINIWEBSERVER_TIMER_H

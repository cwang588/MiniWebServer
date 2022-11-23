//
// Created by chenghao on 11/22/22.
//

#include <cassert>
#include"../header/timer.h"

TimerList::~TimerList() {
    UtilTimer *now = head_;
    while (now != nullptr) {
        now = now->next_;
        delete head_;
        head_ = now;
    }
}

void TimerList::AddTimer(UtilTimer *timer) {
    if (!timer)return;
    if (!head_) {
        head_ = tail_ = timer;
        return;
    }
    if (timer->expire_ < head_->expire_) {
        timer->next_ = head_;
        head_->prev_ = timer;
        head_ = timer;
        return;
    }
    AddTimer(timer, head_);
}

void TimerList::AddTimer(UtilTimer *timer, UtilTimer *list_head) {
    if (!list_head)return;
    UtilTimer *prev = list_head, *tmp = prev->next_;
    while (tmp) {
        if (timer->expire_ < tmp->expire_) {
            prev->next_ = timer;
            timer->prev_ = prev;
            timer->next_ = tmp;
            tmp->prev_ = timer;
            break;
        }
        prev = tmp;
        tmp = prev->next_;
    }
    if (!tmp) {
        prev->next_ = timer;
        timer->prev_ = prev;
        timer->next_ = nullptr;
        tail_ = timer;
    }
}

void TimerList::AdjustTimer(UtilTimer *timer) {
    if (!timer)return;
    UtilTimer *tmp = timer->next_;
    if (!tmp || (timer->expire_ < tmp->expire_))return;
    if (timer == head_) {
        head_ = head_->next_;
        head_->prev_ = nullptr;
        timer->next_ = nullptr;
        AddTimer(timer, head_);
    } else {
        timer->prev_->next_ = timer->next_;
        timer->next_->prev_ = timer->prev_;
        AddTimer(timer, timer->next_);
    }
}

void TimerList::DelTimer(UtilTimer *timer) {
    if (!timer)return;
    if (timer == head_ && timer == tail_) {
        delete timer;
        head_ = tail_ = nullptr;
        return;
    }
    if (timer == head_) {
        head_ = head_->next_;
        head_->prev_ = nullptr;
        delete timer;
        return;
    }
    if (timer == tail_) {
        tail_ = tail_->prev_;
        tail_->next_ = nullptr;
        delete timer;
        return;
    }
    timer->prev_->next_ = timer->next_;
    timer->next_->prev_ = timer->prev_;
    delete timer;
}

void TimerList::Tick() {
    if (!head_)return;
    time_t current = time(NULL);
    UtilTimer *tmp = head_;
    while (tmp) {
        if (current < tmp->expire_)break;
        tmp->cb_func(tmp->user_data);
        head_ = tmp->next_;
        if (head_)head_->prev_ = nullptr;
        delete tmp;
        tmp = head_;
    }
}

void Utils::Init(int timeslot) {
    time_slot_ = timeslot;
}

int Utils::SetNonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::AddFd(int epoll_fd, int fd, bool one_shot, int trig_mode) {
    epoll_event event;
    event.data.fd = fd;
    if (trig_mode == HttpConnection::ET) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot)event.events |= EPOLLONESHOT;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    SetNonBlocking(fd);
}

void Utils::SignalHandler(int signal) {
    int save_errno = errno;
    int msg = signal;
    send(pipe_fd_[1], (char *) &msg, 1, 0);
    errno = save_errno;
}

void Utils::AddSignal(int signal, void (*handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (restart)sa.sa_flags |= SA_RESTART;
    assert(sigaction(signal, &sa, nullptr) != -1);
}

void Utils::TimerHandler() {
    timer_list_.Tick();
    alarm(time_slot_);
}

void Utils::ShowError(int connection_fd, const char *info) {
    send(connection_fd, info, strlen(info), 0);
    close(connection_fd);
}

int *Utils::pipe_fd_ = 0;
int Utils::epoll_fd_ = 0;

class Utils;

void cb_func(ClientData *user_data) {
    epoll_ctl(Utils::epoll_fd_, EPOLL_CTL_DEL, user_data->socket_fd_, 0);
    assert(user_data);
    close(user_data->socket_fd_);
    --HttpConnection::user_count_;
}
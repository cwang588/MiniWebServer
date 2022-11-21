//
// Created by chenghao on 11/9/22.
//
#include "../header/http.h"
#include <mysql/mysql.h>
#include <fstream>
#include <string>


const char *kOk200Title = "OK";
const char *kError400Title = "400 Bad Request";
const char *kError400Form = "Syntax error\n";
const char *kError403Title = "403 Forbidden";
const char *kError403Form = "No permission\n";
const char *kError404Title = "404 Not Found";
const char *kError404Form = "File not found\n";
const char *kError500Title = "500 Internal Error";
const char *kError500Form = "Server internal error\n";

std::map<std::string, std::string> users;
Locker locker;

void HttpConnection::InitMysqlResult(ConnectionPool *connection_pool) {
    MYSQL *mysql = nullptr;
    ConnectionRAII mysql_connection(&mysql, connection_pool);
    if (mysql_query(mysql, "SELECT username, passwd, FROM user")) {
        //todo:report error
    }
    MYSQL_RES *result = mysql_store_result(mysql);
    int num_fields = mysql_num_fields(result);
    MYSQL_FIELD *fields = mysql_fetch_fields(result);
    while (MYSQL_ROW row = mysql_fetch_row(result))users[row[0]] = row[1];
}

int SetNonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL), new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void AddFd(int epoll_fd, int fd, bool one_shot, int trig_mode) {
    epoll_event event;
    event.data.fd = fd;
    if (trig_mode == 1)event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot)event.events |= EPOLLONESHOT;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    SetNonBlocking(fd);
}

void RemoveFd(int epoll_fd, int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void ModFd(int epoll_fd, int fd, int ev, int trig_mode) {
    epoll_event event;
    event.data.fd = fd;
    if (trig_mode == 1)event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConnection::user_count_=0;
int HttpConnection::epoll_fd_=-1;

void HttpConnection::CloseConnection(bool real_close) {
    if(real_close&&socket_fd_!=-1){
        printf("close %d\n",socket_fd_);
        RemoveFd(epoll_fd_,socket_fd_);
        socket_fd_=-1;
        --user_count_;
    }
}

void HttpConnection::Init(int socket_fd, const sockaddr_in &address, char *, int, int, std::string user,
                          std::string password, std::string sql_name) {
    socket_fd_=socket_fd;
}
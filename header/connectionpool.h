//
// Created by chenghao on 11/19/22.
//

#ifndef MINIWEBSERVER_CONNECTIONPOOL_H
#define MINIWEBSERVER_CONNECTIONPOOL_H

#include<mysql/mysql.h>
#include<string>
#include<list>
#include"locker.h"

class ConnectionPool {
public:
    MYSQL *GetConnection();

    bool ReleaseConnection(MYSQL *connection);

    int GetFreeConnection();

    void DestroyPool();

    static ConnectionPool *GetInstance();

    void init(std::string url, std::string user, std::string password, std::string database_name, int port,
              int max_connection, int close_log);

private:
    ConnectionPool() : current_connection(0), free_connection(0){}

    ~ConnectionPool();

public:
    std::string url_, port_, user_, password_, database_name_;
    int close_log_;
private:
    int max_connection, current_connection, free_connection;
    Locker locker_;
    std::list<MYSQL *> connection_pool_;
    Semaphore reserve_;
};


#endif //MINIWEBSERVER_CONNECTIONPOOL_H

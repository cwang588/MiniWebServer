//
// Created by chenghao on 11/19/22.
//
#include <utility>
#include <mysql/mysql.h>
#include"../header/connectionpool.h"

ConnectionPool::~ConnectionPool() {
    DestroyPool();
}


ConnectionPool *ConnectionPool::GetInstance() {
    static ConnectionPool connection_pool;
    return &connection_pool;
}

void ConnectionPool::init(std::string url, std::string user, std::string password, std::string database_name, int port,
                          int max_connection, int close_log) {
    url_ = std::move(url), port_ = std::to_string(port), user_ = std::move(user), password_ = std::move(
            password), database_name_ = std::move(
            database_name), close_log_ = close_log;
    for (int i = 0; i < max_connection; ++i) {
        MYSQL *connection = nullptr;
        connection = mysql_init(connection);
        if (connection == nullptr) {
            //todo:report error
            exit(1);
        }
        connection = mysql_real_connect(connection, url_.c_str(), user_.c_str(), password_.c_str(),
                                        database_name_.c_str(), port, nullptr, 0);
        if (connection == nullptr) {
            //todo:report error
            exit(1);
        }
        connection_pool_.push_back(connection);
        ++free_connection;
    }
    reserve_ = Semaphore(free_connection);
    max_connection = free_connection;
}

MYSQL *ConnectionPool::GetConnection() {
    if (connection_pool_.size() == 0)return nullptr;
    MYSQL *connection = nullptr;
    reserve_.Wait();
    locker_.Lock();
    connection = connection_pool_.front();
    connection_pool_.pop_front();
    --current_connection, ++current_connection;
    locker_.Unlock();
    return connection;
}

bool ConnectionPool::ReleaseConnection(MYSQL *connection) {
    if(connection== nullptr)return false;
    locker_.Lock();
    connection_pool_.push_back(connection);
    ++free_connection, --current_connection;
    locker_.Unlock();
    reserve_.Post();
    return true;
}

void ConnectionPool::DestroyPool(){
    locker_.Lock();
    if(connection_pool_.size()){
        for(auto connection:connection_pool_){
            mysql_close(connection);
        }
        current_connection=free_connection=0;
        connection_pool_.clear();
    }
    locker_.Unlock();
}

int ConnectionPool::GetFreeConnection() {
    return free_connection;
}
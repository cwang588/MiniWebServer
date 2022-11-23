//
// Created by chenghao on 11/22/22.
//

#ifndef MINIWEBSERVER_LOG_H
#define MINIWEBSERVER_LOG_H

#include<cstring>
#include<string>
#include<stdarg.h>
#include"blockqueue.h"
#include"locker.h"

class Log {
public:
    static Log *GetInstance();

    static void *FlushLogThread(void *args);

    bool
    Init(const char *file_name, int close_log, int log_buf_size = 8192, int max_lines = 5000000, int queue_size = 0);

    void WriteLog(int level, const char *format, ...);

    void Flush();

private:
    Log() : line_count_(0), is_async_(false) {}

    virtual ~Log();

    void *AsyncWriteLog();

private:
    char dir_name_[1 << 7], log_name_[1 << 7];
    int max_lines_, log_buf_size_, today_;
    long long line_count_;
    FILE *fp_;
    char *buffer_;
    BlockQueue<std::string> *log_queue_;
    bool is_async_;
    Locker locker_;
    int close_log_;
};

#define LOG_DEBUG(format, ...) if(!close_log_) {Log::GetInstance()->WriteLog(0, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_INFO(format, ...) if(!close_log_) {Log::GetInstance()->WriteLog(1, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_WARN(format, ...) if(!close_log_) {Log::GetInstance()->WriteLog(2, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_ERROR(format, ...) if(!close_log_) {Log::GetInstance()->WriteLog(3, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}

#endif //MINIWEBSERVER_LOG_H

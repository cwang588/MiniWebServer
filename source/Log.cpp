//
// Created by chenghao on 11/22/22.
//
#include"../header/log.h"

Log::~Log() {
    if (fp_ != nullptr)fclose(fp_);
}

bool Log::Init(const char *file_name, int close_log, int log_buf_size, int max_lines, int queue_size) {
    if (queue_size < 0)return false;
    if (queue_size > 0) {
        is_async_ = true;
        log_queue_ = new BlockQueue<std::string>(queue_size);
        pthread_t tid;
        pthread_create(&tid, nullptr, FlushLogThread, nullptr);
    }
    close_log_ = close_log;
    log_buf_size_ = log_buf_size;
    buffer_ = (char *) malloc(log_buf_size_ * sizeof(char));
    memset(buffer_, 0, log_buf_size_);
    max_lines_ = max_lines;
    time_t t = time(nullptr);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};
    if (p == nullptr) {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 file_name);
    } else {
        strcpy(log_name_, p + 1);
        strncpy(dir_name_, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name_, my_tm.tm_year + 1900, my_tm.tm_mon + 1,
                 my_tm.tm_mday, log_name_);
    }
    today_ = my_tm.tm_mday;
    fp_ = fopen(log_full_name, "a");
    if (fp_ == nullptr)return false;
    return true;
}

void Log::WriteLog(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[error]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }
    locker_.Lock();
    line_count_++;

    if (today_ != my_tm.tm_mday || line_count_ % max_lines_ == 0) //everyday log
    {

        char new_log[256] = {0};
        fflush(fp_);
        fclose(fp_);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (today_ != my_tm.tm_mday) {
            snprintf(new_log, 255, "%s%s%s", dir_name_, tail, log_name_);
            today_ = my_tm.tm_mday;
            line_count_ = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name_, tail, log_name_, line_count_ / max_lines_);
        }
        fp_ = fopen(new_log, "a");
    }
    locker_.Unlock();
    va_list va_lst;
    va_start(va_lst, format);
    std::string log_str;
    locker_.Lock();

    int n = snprintf(buffer_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(buffer_ + n, log_buf_size_ - 1, format, va_lst);
    buffer_[n + m] = '\n';
    buffer_[n + m + 1] = '\0';
    log_str = buffer_;
    locker_.Unlock();
    if (is_async_ && !log_queue_->Full()) {
        log_queue_->Push(log_str);
    } else {
        locker_.Lock();
        fputs(log_str.c_str(), fp_);
        locker_.Unlock();
    }

    va_end(va_lst);
}


void Log::Flush(void) {
    locker_.Lock();
    fflush(fp_);
    locker_.Unlock();
}

Log *Log::GetInstance() {
    static Log instance;
    return &instance;
}

void *Log::FlushLogThread(void *args) {
    Log::GetInstance()->AsyncWriteLog();
}

void *Log::AsyncWriteLog() {
    std::string single_log;
    while (log_queue_->Pop(single_log)) {
        locker_.Lock();
        fputs(single_log.c_str(), fp_);
        locker_.Unlock();
    }
}
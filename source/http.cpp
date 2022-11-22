//
// Created by chenghao on 11/9/22.
//
#include "../header/http.h"


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

int HttpConnection::user_count_ = 0;
int HttpConnection::epoll_fd_ = -1;

void HttpConnection::CloseConnection(bool real_close) {
    if (real_close && socket_fd_ != -1) {
        printf("close %d\n", socket_fd_);
        RemoveFd(epoll_fd_, socket_fd_);
        socket_fd_ = -1;
        --user_count_;
    }
}

void HttpConnection::Init(int socket_fd, const sockaddr_in &address, char *document_root, int trig_mode, int close_log,
                          std::string user, std::string password, std::string sql_name) {
    socket_fd_ = socket_fd;
    address_ = address;
    AddFd(epoll_fd_, socket_fd_, true, trig_mode_);
    ++user_count_;
    document_root_ = document_root;
    trig_mode_ = trig_mode;
    close_log_ = close_log;
    strcpy(sql_user_, user.c_str());
    strcpy(sql_name_, sql_name.c_str());
    strcpy(sql_password_, password.c_str());
    Init();
}

void HttpConnection::Init() {
    mysql_ = nullptr;
    bytes_have_sent_ = bytes_to_send_ = 0;
    check_state_ = CHECK_STATE_REQUEST_LINE;
    linger_ = false;
    method_ = GET;
    url_ = version_ = host_ = file_address_ = document_root_ = nullptr;
    content_length_ = check_index_ = read_index_ = write_index_ = cgi_ = state_ = start_line_ = timer_flag_ = improv_ = 0;
    memset(read_buffer_, 0, sizeof(read_buffer_));
    memset(write_buffer_, 0, sizeof(write_buffer_));
    memset(real_file_, 0, sizeof(real_file_));
}

HttpConnection::LineStatus HttpConnection::ParseLine() {
    for (; check_index_ < read_index_; ++check_index_) {
        char tmp = read_buffer_[check_index_];
        switch (tmp) {
            case '\r': {
                if (check_index_ == read_index_ - 1)return LINE_OPEN;
                else if (read_buffer_[check_index_ + 1] == '\n') {
                    read_buffer_[check_index_++] = 0;
                    read_buffer_[check_index_++] = 0;
                    return LINE_OK;
                } else return LINE_BAD;
            }
            case '\n': {
                if (check_index_ > 1 && read_buffer_[check_index_ - 1] == '\r') {
                    read_buffer_[check_index_ - 1] = 0;
                    read_buffer_[check_index_++] = 0;
                    return LINE_OK;
                } else return LINE_BAD;
            }
            default:
                break;
        }
    }
    return LINE_OPEN;
}

bool HttpConnection::ReadOnce() {
    if (read_index_ >= kReadBufferSize)return false;
    switch (trig_mode_) {
        case ET: {
            while (true) {
                int bytes_read = recv(socket_fd_, read_buffer_ + read_index_, kReadBufferSize - read_index_, 0);
                if (bytes_read < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)break;
                    return false;
                } else if (bytes_read == 0)return false;
                else;
                read_index_ += bytes_read;
            }
            break;
        }
        case IN: {
            int bytes_read = recv(socket_fd_, read_buffer_ + read_index_, kReadBufferSize - read_index_, 0);
            if (bytes_read <= 0)return false;
            read_index_ += bytes_read;
            break;
        }
        default:
            break;
    }
    return true;
}

HttpConnection::HttpCode HttpConnection::ParseRequestLine(char *text) {
    url_ = strpbrk(text, " \t");
    if (url_ == nullptr)return BAD_REQUEST;
    *url_++ = 0;
    char *method = text;
    if (!strcasecmp(text, "GET"))method_ = GET;
    else if (!strcasecmp(text, "POST"))method_ = POST, cgi_ = 1;
    else return BAD_REQUEST;//todo:process requests of other types
    url_ += strspn(url_, " \t");
    version_ = strpbrk(url_, " \t");
    if (!version_)return BAD_REQUEST;
    *version_++ = 0;
    version_ += strspn(version_, " \t");
    if (strcasecmp(version_, "HTTP/1.1"))return BAD_REQUEST;
    if (!strncasecmp(url_, "http://", 7)) {
        url_ += 7;
        url_ = strchr(url_, '/');
    }
    if (!strncasecmp(url_, "https://", 8)) {
        url_ += 8;
        url_ = strchr(url_, '/');
    }
    if (!url_ || url_[0] != '/')return BAD_REQUEST;
    if (strlen(url_) == 1)strcat(url_, "judge.html");
    check_state_ = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpConnection::HttpCode HttpConnection::ParseRequestHeader(char *text) {
    if (text[0] == 0) {
        if (content_length_) {
            check_state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strncasecmp(text, "keep-alive", 11) == 0) linger_ = 0;
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        content_length_ = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        host_ = text;
    } else {
        //todo: log error information
    }
    return NO_REQUEST;
}

HttpConnection::HttpCode HttpConnection::ParseRequestContent(char *text) {
    if (read_index_ >= (content_length_ + check_index_)) {
        text[content_length_] = 0;
        string_ = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpConnection::HttpCode HttpConnection::ProcessRead() {
    LineStatus line_status = LINE_OK;
    HttpCode ret = NO_REQUEST;
    char *text = nullptr;
    while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = ParseLine()) == LINE_OK) {
        text = GetLine();
        start_line_ = check_index_;
        //todo: log information
        switch (check_state_) {
            case CHECK_STATE_REQUEST_LINE: {
                ret = ParseRequestLine(text);
                if (ret == BAD_REQUEST)return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = ParseRequestHeader(text);
                if (ret == BAD_REQUEST)return BAD_REQUEST;
                else if (ret == GET_REQUEST)return DoRequest();
                else;
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = ParseRequestContent(text);
                if (ret == GET_REQUEST)return DoRequest();
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpConnection::HttpCode HttpConnection::DoRequest() {
    strcpy(real_file_, document_root_);
    int len = strlen(document_root_);
    const char *p = strrchr(url_, '/');
    if (cgi_ == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
        char flag = url_[1];
        char *url_real = (char *) malloc(sizeof(char) * 200);
        strcpy(url_real, "/");
        strcat(url_real, url_ + 2);
        strncpy(real_file_ + len, url_real, kFileNameLen - len - 1);
        free(url_real);
        char name[100], password[100];
        int i, j = 0;
        for (i = 5; string_[i] != '&'; ++i)name[i - 5] = string_[i];
        for (i = i + 10; string_[i] != 0; ++i, ++j)password[j] = string_[i];
        password[j] = 0;
        if (*(p + 1) == '3') {
            if (users_.find(name) == users_.end())strcpy(url_, "/registerError.html");
            else {
                char *sql_insert = (char *) malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");
                locker.Lock();
                int res = mysql_query(mysql_, sql_insert);
                locker.Unlock();
                if (!res)strcpy(url_, "/registerError.html");
                else {
                    users[name] = password;
                    strcpy(url_, "/log.html");
                }
            }
        } else if (*(p + 1) == '2') {
            if (users.find(name) != users.end() && users[name] == password)strcpy(url_, "/welcome.html");
            else strcpy(url_, "logError.html");
        }
    }
    switch (*(p + 1)) {
        case '0': {
            char *url_real = (char *) malloc(sizeof(char) * 200);
            strcpy(url_real, "/register.html");
            strncpy(real_file_ + len, url_real, strlen(url_real));
            free(url_real);
            break;
        }
        case '1': {
            char *url_real = (char *) malloc(sizeof(char) * 200);
            strcpy(url_real, "/log.html");
            strncpy(real_file_ + len, url_real, strlen(url_real));
            free(url_real);
            break;
        }
        case '5': {
            char *url_real = (char *) malloc(sizeof(char) * 200);
            strcpy(url_real, "/picture.html");
            strncpy(real_file_ + len, url_real, strlen(url_real));
            free(url_real);
            break;
        }
        case '6': {
            char *url_real = (char *) malloc(sizeof(char) * 200);
            strcpy(url_real, "/video.html");
            strncpy(real_file_ + len, url_real, strlen(url_real));
            free(url_real);
            break;
        }
        case '7': {
            char *url_real = (char *) malloc(sizeof(char) * 200);
            strcpy(url_real, "/fans.html");
            strncpy(real_file_ + len, url_real, strlen(url_real));
            free(url_real);
            break;
        }
        default:
            strncpy(real_file_ + len, url_, kFileNameLen - len - 1);
    }
    if (stat(real_file_, &file_stat_) < 0)return NO_RESOURCE;
    if (!(file_stat_.st_mode & S_IROTH))return FORBIDDEN_REQUEST;
    if (S_ISDIR(file_stat_.st_mode))return BAD_REQUEST;
    int fd = open(real_file_, O_RDONLY);
    file_address_ = (char *) mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void HttpConnection::Unmap() {
    if (file_address_) {
        munmap(file_address_, file_stat_.st_size);
        file_address_ = 0;
    }
}

bool HttpConnection::Write() {
    if (!bytes_to_send_) {
        ModFd(epoll_fd_, socket_fd_, EPOLLIN, trig_mode_);
        Init();
        return true;
    }
    while (true) {
        int tmp = writev(socket_fd_, iv_, iv_count_);
        if (tmp < 0) {
            if (errno == EAGAIN) {
                ModFd(epoll_fd_, socket_fd_, EPOLLOUT, trig_mode_);
                return true;
            }
            Unmap();
            return false;
        }
        bytes_have_sent_ += tmp;
        bytes_to_send_ -= tmp;
        if (bytes_have_sent_ >= iv_[0].iov_len) {
            iv_[0].iov_len = 0;
            iv_[1].iov_base = file_address_ + (bytes_have_sent_ - write_index_);
            iv_[1].iov_len = bytes_to_send_;
        } else {
            iv_[0].iov_base = write_buffer_ + bytes_have_sent_;
            iv_[0].iov_len = iv_[0].iov_len - bytes_have_sent_;
        }
        if (bytes_to_send_ <= 0) {
            Unmap();
            ModFd(epoll_fd_, socket_fd_, EPOLLIN, trig_mode_);
            if (linger_) {
                Init();
                return true;
            }
            return false;
        }
    }
}

bool HttpConnection::AddResponse(const char *format, ...) {
    if (write_index_ >= kWriteBufferSize)return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buffer_ + write_index_, kWriteBufferSize - 1 - write_index_, format, arg_list);
    if (len >= kWriteBufferSize - 1 - write_index_) {
        va_end(arg_list);
        return false;
    }
    write_index_ += len;
    va_end(arg_list);
    //todo:log information
    return true;
}

bool HttpConnection::AddStatusLine(int status, const char *title) {
    return AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConnection::AddHeader(int content_length) {
    return AddContentLength(content_length) && AddLinger() && AddBlankLine();
}

bool HttpConnection::AddContentLength(int content_length) {
    return AddResponse("Content-Length", content_length);
}

bool HttpConnection::AddContentType() {
    return AddResponse("Content-Type:%s\r\n", "text/html");
}

bool HttpConnection::AddLinger() {
    return AddResponse("Connection:%s\r\n", linger_ ? "keep-alive" : "close");
}

bool HttpConnection::AddBlankLine() {
    return AddResponse("%s", "\r\n");
}

bool HttpConnection::AddContent(const char *content) {
    return AddResponse("%s", content);
}

bool HttpConnection::ProcessWrite(HttpConnection::HttpCode ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            AddStatusLine(500, kError500Title);
            AddHeader(strlen(kError500Form));
            if (!AddContent(kError400Form))return false;
            break;
        }
        case BAD_REQUEST: {
            AddStatusLine(404, kError404Title);
            AddHeader(strlen(kError404Form));
            if (!AddContent(kError404Form))return false;
            break;
        }
        case FORBIDDEN_REQUEST: {
            AddStatusLine(403, kError403Title);
            AddHeader(strlen(kError403Form));
            if (!AddContent(kError403Form))return false;
            break;
        }
        case FILE_REQUEST: {
            AddStatusLine(200, kOk200Title);
            if (file_stat_.st_size) {
                AddHeader(file_stat_.st_size);
                iv_[0].iov_base = write_buffer_;
                iv_[0].iov_len = write_index_;
                iv_[1].iov_base = file_address_;
                iv_[1].iov_len = file_stat_.st_size;
                iv_count_ = 2;
                bytes_to_send_ = write_index_ + file_stat_.st_size;
                return true;
            } else {
                const char *ok_string = "<html><body></body></html>";
                AddHeader(strlen(ok_string));
                if (!AddContent(ok_string))return false;
            }
        }
        default:
            return false;
    }
    iv_[0].iov_base = write_buffer_;
    iv_[0].iov_len = write_index_;
    iv_count_ = 1;
    bytes_to_send_ = write_index_;
    return true;
}

void HttpConnection::Process() {
    HttpCode read_ret = ProcessRead();
    if (read_ret == NO_REQUEST) {
        ModFd(epoll_fd_, socket_fd_, EPOLLIN, trig_mode_);
        return;
    }
    bool write_ret = ProcessWrite(read_ret);
    if (!write_ret)CloseConnection();
    ModFd(epoll_fd_, socket_fd_, EPOLLOUT, trig_mode_);
}

char * HttpConnection::GetLine() {
    return read_buffer_ + start_line_;
}
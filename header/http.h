//
// Created by chenghao on 11/8/22.
//

#ifndef MINIWEBSERVER_HTTP_H
#define MINIWEBSERVER_HTTP_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <map>
#include <mysql/mysql.h>

class HttpConnection {
public:
    static const int kFileNameLen=200;
    static const int kReadBufferSize=2048;
    const int kWriteBufferSize=1024;
    enum Method {
        GET,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CheckState {
        CHECK_STATE_REQUEST_LINE,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HttpCode {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LineStatus {
        LINE_OK,
        LINE_BAD,
        LINE_OPEN
    };
public:
    HttpConnection();

    ~HttpConnection();

    void Init(int socket_fd, const sockaddr_in &address, char *, int, int, std::string user, std::string password,
              std::string sql_name);

    void CloseConnection(bool real_close = true);

    void Process();

    bool ReadOnce();

    bool Write();

    sockaddr_in *GetAddress();

    void InitMysqlResult(ConnectionPool *connection_pool);

private:
    void Init();

    HttpCode ProcessRead();

    bool ProcessWrite(HttpCode ret);

    HttpCode ParseRequestLine(char *text);

    HttpCode ParseRequestHeader(char *text);

    HttpCode ParseRequestContent(char *text);

    HttpCode DoRequest();

    char *GetLine();

    LineStatus ParseLine();

    void Unmap();

    bool AddResponse(const char *format, ...);

    bool AddContent(const char *content);

    bool AddStatusLine(int status, const char *title);

    bool AddHeader(int content_length);

    bool AddContentType();

    bool AddContentLength(int content_length);

    bool AddLinger();

    bool AddBlankLine();
public:
    static int epoll_fd_;
    static int user_count_;
    MYSQL *mysql;
    int state_;
private:
    int socket_fd_;
    sockaddr_in address_;
    char read_buffer_;
    int read_index_;
    int check_index_;
    int start_line_;
    char write_buf_[kReadBufferSize];
    int write_index_;
    CheckState check_state_;
    Method method;
    char real_file_[kFileNameLen];
    char *url_;
    char *version_;
    char *host_;
    int content_length_;
    bool linger_;
    char *file_address_;


    int cgi_;
    char *string_;
    int bytes_to_send_;
    int bytes_have_sent_;
    char *document_root_;
    std::map<std::string,std::string>users_;
    int trig_mode_;
    int close_log_;
    char sql_user_[105];
    char sql_password_[105];
    char sql_name_[105];
};

#endif //MINIWEBSERVER_HTTP_H

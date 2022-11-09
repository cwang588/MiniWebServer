//
// Created by chenghao on 11/9/22.
//
#include "../header/http.h"
#include <mysql/mysql.h>
#include <fstream>

const char *kOk200Title = "OK";
const char *kError400Title = "400 Bad Request";
const char *kError400Form = "Syntax error\n";
const char *kError403Title = "403 Forbidden";
const char *kError403Form = "No permission\n";
const char *kError404Title = "404 Not Found";
const char *kError404Form = "File not found\n";
const char *kError500Title = "500 Internal Error";
const char *kError500Form = "Server internal error\n";

std::map <std::string, std::string> users;

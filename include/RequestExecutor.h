#ifndef REQUEST_EXECUTOR_H
#define REQUEST_EXECUTOR_H

#include <string>
#include <vector>

struct Request
{
    std::string op;
    std::vector<std::string> args;
};

class RequestExecutor
{
private:
    int reserve_delay;
    int cancel_delay;
    int check_delay;

    bool parse_date_duration(const std::vector<std::string> &args, int base, int &start, int &end, std::string &reason);
    std::string device_list_to_string(const std::vector<int> &device_indices);

    void execute_reserve(const Request &req);
    void execute_reserveblock(const Request &req);
    void execute_reserveany(const Request &req);
    void execute_cancel(const Request &req);
    void execute_cancelblock(const Request &req);
    void execute_cancelany(const Request &req);
    void execute_check(const Request &req);

public:
    RequestExecutor(int r, int c, int ch);
    void execute(const Request &req);
};

#endif

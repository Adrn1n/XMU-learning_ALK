#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <string>
#include <vector>
#include "RequestExecutor.h"

struct UserProgram
{
    int reserve_delay;
    int cancel_delay;
    int check_delay;
    std::vector<Request> requests;
};

class InputParser
{
public:
    static bool parse(const std::string &filename, std::vector<int> &device_ids, std::vector<UserProgram> &programs);
};

#endif

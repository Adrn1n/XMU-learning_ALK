#include "InputParser.h"
#include "config.h"
#include <iostream>
#include <fstream>

bool InputParser::parse(const std::string &filename, std::vector<int> &device_ids, std::vector<UserProgram> &programs)
{
    std::ifstream fin(filename.c_str());
    if (!fin)
    {
        std::cerr << "cannot open input file: " << filename << std::endl;
        return false;
    }

    int n;
    fin >> n;
    if (!fin || n <= 0 || n > MAX_DEVICES)
    {
        std::cerr << "invalid device count" << std::endl;
        return false;
    }

    device_ids.resize(n);
    for (int i = 0; i < n; ++i)
        fin >> device_ids[i];

    int m;
    fin >> m;
    if (!fin || m <= 0 || m > MAX_USERS)
    {
        std::cerr << "invalid user count" << std::endl;
        return false;
    }

    programs.clear();
    for (int i = 0; i < m; ++i)
    {
        std::string token;
        fin >> token;
        if (token != "user")
        {
            std::cerr << "expected user, got " << token << std::endl;
            return false;
        }

        UserProgram up;
        up.reserve_delay = 0;
        up.cancel_delay = 0;
        up.check_delay = 0;
        std::string key;

        fin >> key >> up.reserve_delay;
        if (key != "reserve")
        {
            std::cerr << "expected reserve delay" << std::endl;
            return false;
        }
        fin >> key >> up.cancel_delay;
        if (key != "cancel")
        {
            std::cerr << "expected cancel delay" << std::endl;
            return false;
        }
        fin >> key >> up.check_delay;
        if (key != "check")
        {
            std::cerr << "expected check delay" << std::endl;
            return false;
        }

        while (fin >> token)
        {
            if (token == "end." || token == "end")
                break;

            Request req;
            req.op = token;
            int arg_count = -1;

            if (token == "reserve" || token == "reserveany" || token == "cancel" || token == "cancelany")
                arg_count = 6;
            else if (token == "reserveblock" || token == "cancelblock")
                arg_count = 7;
            else if (token == "check")
                arg_count = 1;
            else
            {
                std::cerr << "unknown operation in input: " << token << std::endl;
                return false;
            }

            for (int j = 0; j < arg_count; ++j)
            {
                std::string arg;
                fin >> arg;
                req.args.push_back(arg);
            }
            up.requests.push_back(req);
        }
        programs.push_back(up);
    }
    return true;
}

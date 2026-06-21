#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <string>

class UserManager
{
public:
    static int get_or_create_user(const std::string &name);
    static int find_user(const std::string &name);
    static std::string get_user_name(int user_id);
};

#endif

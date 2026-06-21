#include "UserManager.h"
#include "SharedMemoryManager.h"
#include "config.h"
#include <cstring>

int UserManager::get_or_create_user(const std::string &name)
{
    for (int i = 0; i < g_shm->user_count; ++i)
    {
        if (g_shm->users[i].valid && name == g_shm->users[i].name)
        {
            return i;
        }
    }
    if (g_shm->user_count >= MAX_USERS)
        return -1;

    int id = g_shm->user_count++;
    g_shm->users[id].valid = 1;
    memset(g_shm->users[id].name, 0, MAX_NAME_LEN);
    strncpy(g_shm->users[id].name, name.c_str(), MAX_NAME_LEN - 1);
    return id;
}

int UserManager::find_user(const std::string &name)
{
    for (int i = 0; i < g_shm->user_count; ++i)
    {
        if (g_shm->users[i].valid && name == g_shm->users[i].name)
        {
            return i;
        }
    }
    return -1;
}

std::string UserManager::get_user_name(int user_id)
{
    if (user_id >= 0 && user_id < g_shm->user_count && g_shm->users[user_id].valid)
    {
        return g_shm->users[user_id].name;
    }
    return "unknown";
}

#ifndef SHARED_MEMORY_MANAGER_H
#define SHARED_MEMORY_MANAGER_H

#include <vector>
#include "config.h"

extern SharedData *g_shm;

class SharedMemoryManager
{
public:
    static bool init(const std::vector<int> &device_ids);
    static void destroy();
};

#endif

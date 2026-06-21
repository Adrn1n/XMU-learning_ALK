#include "SharedMemoryManager.h"
#include <iostream>
#include <cstring>
#include <sys/mman.h>
#include <algorithm>

SharedData *g_shm = nullptr;

bool SharedMemoryManager::init(const std::vector<int> &device_ids)
{
    void *mem = mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        perror("mmap");
        return false;
    }

    g_shm = static_cast<SharedData *>(mem);
    memset(g_shm, 0, sizeof(SharedData));

    if (sem_init(&g_shm->resource_sem, 1, 1) != 0 ||
        sem_init(&g_shm->read_mutex_sem, 1, 1) != 0 ||
        sem_init(&g_shm->print_sem, 1, 1) != 0)
    {
        perror("sem_init");
        return false;
    }

    g_shm->read_count = 0;
    g_shm->device_count = static_cast<int>(device_ids.size());

    for (size_t i = 0; i < device_ids.size(); ++i)
    {
        g_shm->devices[i].valid = 1;
        g_shm->devices[i].device_id = device_ids[i];
    }

    std::sort(g_shm->devices, g_shm->devices + g_shm->device_count, [](const DeviceEntry &a, const DeviceEntry &b)
              { return a.device_id < b.device_id; });

    return true;
}

void SharedMemoryManager::destroy()
{
    if (!g_shm)
        return;
    sem_destroy(&g_shm->resource_sem);
    sem_destroy(&g_shm->read_mutex_sem);
    sem_destroy(&g_shm->print_sem);
    munmap(g_shm, sizeof(SharedData));
    g_shm = nullptr;
}

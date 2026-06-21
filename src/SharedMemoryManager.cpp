#include "SharedMemoryManager.h"
#include <iostream>
#include <cstring>
#include <sys/mman.h>
#include <algorithm>
#include <string>   // 新增
#include <unistd.h> // 新增，用于 getpid()

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

#ifdef __APPLE__
    // macOS: 使用以 PID 命名的唯一有名信号量
    std::string res_name = "/sem_res_" + std::to_string(getpid());
    std::string read_name = "/sem_read_" + std::to_string(getpid());
    std::string print_name = "/sem_print_" + std::to_string(getpid());

    g_shm->resource_sem = sem_open(res_name.c_str(), O_CREAT | O_EXCL, 0644, 1);
    g_shm->read_mutex_sem = sem_open(read_name.c_str(), O_CREAT | O_EXCL, 0644, 1);
    g_shm->print_sem = sem_open(print_name.c_str(), O_CREAT | O_EXCL, 0644, 1);

    if (g_shm->resource_sem == SEM_FAILED ||
        g_shm->read_mutex_sem == SEM_FAILED ||
        g_shm->print_sem == SEM_FAILED)
    {
        perror("sem_open");
        return false;
    }

    // 立即 unlink，确保程序退出或崩溃时系统能自动释放它们
    sem_unlink(res_name.c_str());
    sem_unlink(read_name.c_str());
    sem_unlink(print_name.c_str());
#else
    // Linux: 使用进程间共享的无名信号量
    if (sem_init(&g_shm->resource_sem, 1, 1) != 0 ||
        sem_init(&g_shm->read_mutex_sem, 1, 1) != 0 ||
        sem_init(&g_shm->print_sem, 1, 1) != 0)
    {
        perror("sem_init");
        return false;
    }
#endif

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

#ifdef __APPLE__
    if (g_shm->resource_sem != SEM_FAILED)
        sem_close(g_shm->resource_sem);
    if (g_shm->read_mutex_sem != SEM_FAILED)
        sem_close(g_shm->read_mutex_sem);
    if (g_shm->print_sem != SEM_FAILED)
        sem_close(g_shm->print_sem);
#else
    sem_destroy(&g_shm->resource_sem);
    sem_destroy(&g_shm->read_mutex_sem);
    sem_destroy(&g_shm->print_sem);
#endif

    munmap(g_shm, sizeof(SharedData));
    g_shm = nullptr;
}

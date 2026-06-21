#include "SyncManager.h"
#include "SharedMemoryManager.h"
#include <semaphore.h>

void SyncManager::read_lock()
{
    sem_wait_compat(g_shm->read_mutex_sem);
    g_shm->read_count++;
    if (g_shm->read_count == 1)
    {
        sem_wait_compat(g_shm->resource_sem);
    }
    sem_post_compat(g_shm->read_mutex_sem);
}

void SyncManager::read_unlock()
{
    sem_wait_compat(g_shm->read_mutex_sem);
    g_shm->read_count--;
    if (g_shm->read_count == 0)
    {
        sem_post_compat(g_shm->resource_sem);
    }
    sem_post_compat(g_shm->read_mutex_sem);
}

void SyncManager::write_lock()
{
    sem_wait_compat(g_shm->resource_sem);
}

void SyncManager::write_unlock()
{
    sem_post_compat(g_shm->resource_sem);
}

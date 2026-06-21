#include "DeviceManager.h"
#include "SharedMemoryManager.h"
#include "config.h"
#include <algorithm>

int DeviceManager::find_device_index(int device_id)
{
    for (int i = 0; i < g_shm->device_count; ++i)
    {
        if (g_shm->devices[i].valid && g_shm->devices[i].device_id == device_id)
        {
            return i;
        }
    }
    return -1;
}

int DeviceManager::get_device_id_by_index(int idx)
{
    if (idx < 0 || idx >= g_shm->device_count)
        return -1;
    return g_shm->devices[idx].device_id;
}

int DeviceManager::min_device_id()
{
    if (g_shm->device_count == 0)
        return 0;
    int ans = g_shm->devices[0].device_id;
    for (int i = 1; i < g_shm->device_count; ++i)
    {
        ans = std::min(ans, g_shm->devices[i].device_id);
    }
    return ans;
}

int DeviceManager::max_device_id()
{
    if (g_shm->device_count == 0)
        return 0;
    int ans = g_shm->devices[0].device_id;
    for (int i = 1; i < g_shm->device_count; ++i)
    {
        ans = std::max(ans, g_shm->devices[i].device_id);
    }
    return ans;
}

bool DeviceManager::build_block_devices(int count, int first_device_id, std::vector<int> &device_indices)
{
    device_indices.clear();
    if (count <= 0 || count > g_shm->device_count)
        return false;

    int min_id = min_device_id();
    int max_id = max_device_id();
    int cur = first_device_id;

    for (int i = 0; i < count; ++i)
    {
        int idx = find_device_index(cur);
        if (idx < 0)
            return false;
        device_indices.push_back(idx);
        if (cur == max_id)
            cur = min_id;
        else
            cur++;
    }
    return true;
}

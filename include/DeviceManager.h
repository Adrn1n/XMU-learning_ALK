#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <vector>

class DeviceManager
{
public:
    static int find_device_index(int device_id);
    static int get_device_id_by_index(int idx);
    static int min_device_id();
    static int max_device_id();
    static bool build_block_devices(int count, int first_device_id, std::vector<int> &device_indices);
};

#endif

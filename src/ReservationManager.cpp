#include "ReservationManager.h"
#include "SharedMemoryManager.h"
#include "Utils.h"
#include "DeviceManager.h"
#include "UserManager.h"
#include "SyncManager.h"
#include "config.h"
#include <sstream>
#include <iostream>
#include <cstring>
#include <unistd.h>

bool ReservationManager::overlap(int l1, int r1, int l2, int r2)
{
    return l1 < r2 && l2 < r1;
}

void ReservationManager::index_add(int rid)
{
    ReservationEntry &r = g_shm->reservations[rid];
    bit_set(g_shm->device_to_res[r.device_index], rid);
    bit_set(g_shm->user_to_res[r.user_id], rid);
    for (int d = r.start_day; d < r.end_day; ++d)
    {
        bit_set(g_shm->day_to_res[d], rid);
    }
}

void ReservationManager::index_remove(int rid)
{
    ReservationEntry &r = g_shm->reservations[rid];
    bit_clear(g_shm->device_to_res[r.device_index], rid);
    bit_clear(g_shm->user_to_res[r.user_id], rid);
    for (int d = r.start_day; d < r.end_day; ++d)
    {
        bit_clear(g_shm->day_to_res[d], rid);
    }
}

int ReservationManager::add_reservation(int user_id, int device_index, int start_day, int end_day)
{
    for (int i = 0; i < MAX_RESERVATIONS; ++i)
    {
        if (!g_shm->reservations[i].active)
        {
            g_shm->reservations[i].active = 1;
            g_shm->reservations[i].user_id = user_id;
            g_shm->reservations[i].device_index = device_index;
            g_shm->reservations[i].start_day = start_day;
            g_shm->reservations[i].end_day = end_day;
            index_add(i);
            return i;
        }
    }
    return -1;
}

void ReservationManager::remove_reservation(int rid)
{
    if (rid < 0 || rid >= MAX_RESERVATIONS || !g_shm->reservations[rid].active)
        return;
    index_remove(rid);
    memset(&g_shm->reservations[rid], 0, sizeof(ReservationEntry));
}

bool ReservationManager::device_busy(int device_index, int start_day, int end_day)
{
    if (device_index < 0 || device_index >= g_shm->device_count)
        return true;
    for (int rid = 0; rid < MAX_RESERVATIONS; ++rid)
    {
        if (!bit_test(g_shm->device_to_res[device_index], rid))
            continue;
        if (!g_shm->reservations[rid].active)
            continue;
        ReservationEntry &r = g_shm->reservations[rid];
        if (r.device_index == device_index && overlap(r.start_day, r.end_day, start_day, end_day))
        {
            return true;
        }
    }
    return false;
}

bool ReservationManager::can_reserve_all(const std::vector<int> &device_indices, int start_day, int end_day, std::string &reason)
{
    for (size_t i = 0; i < device_indices.size(); ++i)
    {
        int idx = device_indices[i];
        if (idx < 0 || idx >= g_shm->device_count)
        {
            reason = "invalid device";
            return false;
        }
        if (device_busy(idx, start_day, end_day))
        {
            reason = "device busy";
            return false;
        }
    }
    return true;
}

bool ReservationManager::reserve_devices(const std::vector<int> &device_indices, int start_day, int end_day, const std::string &user_name, std::string &reason)
{
    int user_id = UserManager::get_or_create_user(user_name);
    if (user_id < 0)
    {
        reason = "too many users";
        return false;
    }
    if (!can_reserve_all(device_indices, start_day, end_day, reason))
        return false;

    for (size_t i = 0; i < device_indices.size(); ++i)
    {
        if (add_reservation(user_id, device_indices[i], start_day, end_day) < 0)
        {
            reason = "reservation table full";
            return false;
        }
    }
    reason = "success";
    return true;
}

int ReservationManager::find_owned_covering_reservation(int user_id, int device_index, int start_day, int end_day)
{
    for (int rid = 0; rid < MAX_RESERVATIONS; ++rid)
    {
        if (!bit_test(g_shm->device_to_res[device_index], rid))
            continue;
        if (!g_shm->reservations[rid].active)
            continue;
        ReservationEntry &r = g_shm->reservations[rid];
        if (r.user_id == user_id && r.device_index == device_index && r.start_day <= start_day && r.end_day >= end_day)
        {
            return rid;
        }
    }
    return -1;
}

bool ReservationManager::has_other_user_reservation(int user_id, int device_index, int start_day, int end_day)
{
    for (int rid = 0; rid < MAX_RESERVATIONS; ++rid)
    {
        if (!bit_test(g_shm->device_to_res[device_index], rid))
            continue;
        if (!g_shm->reservations[rid].active)
            continue;
        ReservationEntry &r = g_shm->reservations[rid];
        if (r.device_index == device_index && r.user_id != user_id && overlap(r.start_day, r.end_day, start_day, end_day))
        {
            return true;
        }
    }
    return false;
}

bool ReservationManager::can_cancel_all(const std::vector<int> &device_indices, int start_day, int end_day, const std::string &user_name, std::vector<int> &reservation_ids, std::string &reason)
{
    reservation_ids.clear();
    int user_id = UserManager::find_user(user_name);
    if (user_id < 0)
    {
        reason = "no such reservation";
        return false;
    }

    for (size_t i = 0; i < device_indices.size(); ++i)
    {
        int dev = device_indices[i];
        int rid = find_owned_covering_reservation(user_id, dev, start_day, end_day);
        if (rid >= 0)
        {
            reservation_ids.push_back(rid);
            continue;
        }
        if (has_other_user_reservation(user_id, dev, start_day, end_day))
        {
            reason = "permission denied";
            return false;
        }
        reason = "no such reservation";
        return false;
    }
    return true;
}

void ReservationManager::cancel_by_reservation_ids(const std::vector<int> &reservation_ids, int start_day, int end_day)
{
    for (size_t i = 0; i < reservation_ids.size(); ++i)
    {
        int rid = reservation_ids[i];
        if (rid < 0 || rid >= MAX_RESERVATIONS || !g_shm->reservations[rid].active)
            continue;
        ReservationEntry old = g_shm->reservations[rid];
        remove_reservation(rid);
        if (old.start_day < start_day)
        {
            add_reservation(old.user_id, old.device_index, old.start_day, start_day);
        }
        if (end_day < old.end_day)
        {
            add_reservation(old.user_id, old.device_index, end_day, old.end_day);
        }
    }
}

bool ReservationManager::cancel_devices(const std::vector<int> &device_indices, int start_day, int end_day, const std::string &user_name, std::string &reason)
{
    std::vector<int> reservation_ids;
    if (!can_cancel_all(device_indices, start_day, end_day, user_name, reservation_ids, reason))
        return false;
    cancel_by_reservation_ids(reservation_ids, start_day, end_day);
    reason = "success";
    return true;
}

std::vector<int> ReservationManager::find_any_available_devices(int count, int start_day, int end_day)
{
    std::vector<int> result;
    for (int i = 0; i < g_shm->device_count; ++i)
    {
        if (!device_busy(i, start_day, end_day))
        {
            result.push_back(i);
            if ((int)result.size() == count)
                break;
        }
    }
    if ((int)result.size() != count)
        result.clear();
    return result;
}

std::vector<int> ReservationManager::find_any_cancel_devices(int count, int start_day, int end_day, const std::string &user_name)
{
    std::vector<int> result;
    int user_id = UserManager::find_user(user_name);
    if (user_id < 0)
        return result;

    for (int dev = 0; dev < g_shm->device_count; ++dev)
    {
        int rid = find_owned_covering_reservation(user_id, dev, start_day, end_day);
        if (rid >= 0)
        {
            result.push_back(dev);
            if ((int)result.size() == count)
                break;
        }
    }
    if ((int)result.size() != count)
        result.clear();
    return result;
}

void ReservationManager::check_user(const std::string &user_name, int delay)
{
    SyncManager::read_lock();
    sleep(delay);

    std::ostringstream out;
    out << "[PID " << getpid() << "] check " << user_name << "\n";
    int user_id = UserManager::find_user(user_name);
    if (user_id < 0)
    {
        out << "  no reservation";
        print_locked(out.str());
        SyncManager::read_unlock();
        return;
    }

    bool any = false;
    for (int rid = 0; rid < MAX_RESERVATIONS; ++rid)
    {
        if (!bit_test(g_shm->user_to_res[user_id], rid))
            continue;
        if (!g_shm->reservations[rid].active)
            continue;
        ReservationEntry &r = g_shm->reservations[rid];
        if (r.user_id != user_id)
            continue;

        any = true;
        out << "  device_id=" << DeviceManager::get_device_id_by_index(r.device_index)
            << " start=" << DateUtil::day_to_string(r.start_day)
            << " end=" << DateUtil::day_to_string(r.end_day) << "\n";
    }

    if (!any)
        out << "  no reservation";
    print_locked(out.str());
    SyncManager::read_unlock();
}

void ReservationManager::print_final_table()
{
    SyncManager::read_lock();
    std::cout << "\n========== Final Reservation Table ==========" << std::endl;
    bool any = false;

    for (int dev = 0; dev < g_shm->device_count; ++dev)
    {
        int device_id = DeviceManager::get_device_id_by_index(dev);
        std::cout << "Device " << device_id << ":" << std::endl;
        bool dev_any = false;

        for (int rid = 0; rid < MAX_RESERVATIONS; ++rid)
        {
            if (!bit_test(g_shm->device_to_res[dev], rid))
                continue;
            if (!g_shm->reservations[rid].active)
                continue;
            ReservationEntry &r = g_shm->reservations[rid];
            if (r.device_index != dev)
                continue;

            dev_any = true;
            any = true;
            std::cout << "  user=" << UserManager::get_user_name(r.user_id)
                      << " start=" << DateUtil::day_to_string(r.start_day)
                      << " end=" << DateUtil::day_to_string(r.end_day) << std::endl;
        }

        if (!dev_any)
            std::cout << "  empty" << std::endl;
    }

    if (!any)
        std::cout << "No reservations." << std::endl;
    std::cout << "=============================================" << std::endl;
    SyncManager::read_unlock();
}

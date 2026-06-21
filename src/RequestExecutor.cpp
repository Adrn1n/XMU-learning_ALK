#include "RequestExecutor.h"
#include "SharedMemoryManager.h"
#include "Utils.h"
#include "SyncManager.h"
#include "DeviceManager.h"
#include "ReservationManager.h"
#include "config.h"
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <string>

bool parse_int(const std::string &s, int &v)
{
    char *end = nullptr;
    long x = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0')
        return false;
    v = (int)x;
    return true;
}

RequestExecutor::RequestExecutor(int r, int c, int ch) : reserve_delay(r), cancel_delay(c), check_delay(ch) {}

bool RequestExecutor::parse_date_duration(const std::vector<std::string> &args, int base, int &start, int &end, std::string &reason)
{
    int y, m, d, duration;
    if (!parse_int(args[base], y) || !parse_int(args[base + 1], m) || !parse_int(args[base + 2], d) || !parse_int(args[base + 3], duration))
    {
        reason = "invalid argument";
        return false;
    }
    if (duration <= 0)
    {
        reason = "invalid duration";
        return false;
    }
    start = DateUtil::to_day_index(y, m, d);
    if (start < 0)
    {
        reason = "invalid date";
        return false;
    }
    end = start + duration;
    if (end > DAYS_2026_2027)
    {
        reason = "invalid date";
        return false;
    }
    return true;
}

std::string RequestExecutor::device_list_to_string(const std::vector<int> &device_indices)
{
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < device_indices.size(); ++i)
    {
        if (i)
            os << " ";
        os << DeviceManager::get_device_id_by_index(device_indices[i]);
    }
    os << "]";
    return os.str();
}

void RequestExecutor::execute(const Request &req)
{
    if (req.op == "reserve")
        execute_reserve(req);
    else if (req.op == "reserveblock")
        execute_reserveblock(req);
    else if (req.op == "reserveany")
        execute_reserveany(req);
    else if (req.op == "cancel")
        execute_cancel(req);
    else if (req.op == "cancelblock")
        execute_cancelblock(req);
    else if (req.op == "cancelany")
        execute_cancelany(req);
    else if (req.op == "check")
        execute_check(req);
    else
        print_locked("[PID " + std::to_string(getpid()) + "] unknown operation: " + req.op);
}

void RequestExecutor::execute_reserve(const Request &req)
{
    if (req.args.size() != 6)
    {
        print_locked("reserve failed: invalid argument");
        return;
    }
    int device_id;
    parse_int(req.args[0], device_id);
    int start, end;
    std::string reason;
    if (!parse_date_duration(req.args, 1, start, end, reason))
    {
        print_locked("reserve failed: " + reason);
        return;
    }
    std::string user = req.args[5];

    SyncManager::write_lock();
    sleep(reserve_delay);

    int dev = DeviceManager::find_device_index(device_id);
    if (dev < 0)
    {
        print_locked("[PID " + std::to_string(getpid()) + "] reserve failed: invalid device");
        SyncManager::write_unlock();
        return;
    }

    std::vector<int> devices = {dev};
    bool ok = ReservationManager::reserve_devices(devices, start, end, user, reason);
    if (ok)
        print_locked("[PID " + std::to_string(getpid()) + "] reserve success: device " + std::to_string(device_id));
    else
        print_locked("[PID " + std::to_string(getpid()) + "] reserve failed: " + reason);

    SyncManager::write_unlock();
}

void RequestExecutor::execute_reserveblock(const Request &req)
{
    if (req.args.size() != 7)
    {
        print_locked("reserveblock failed: invalid argument");
        return;
    }
    int count, first_device_id;
    parse_int(req.args[0], count);
    parse_int(req.args[1], first_device_id);
    int start, end;
    std::string reason;
    if (!parse_date_duration(req.args, 2, start, end, reason))
    {
        print_locked("reserveblock failed: " + reason);
        return;
    }
    std::string user = req.args[6];

    SyncManager::write_lock();
    sleep(reserve_delay);

    std::vector<int> devices;
    if (!DeviceManager::build_block_devices(count, first_device_id, devices))
    {
        print_locked("[PID " + std::to_string(getpid()) + "] reserveblock failed: invalid device");
        SyncManager::write_unlock();
        return;
    }

    bool ok = ReservationManager::reserve_devices(devices, start, end, user, reason);
    if (ok)
        print_locked("[PID " + std::to_string(getpid()) + "] reserveblock success: devices " + device_list_to_string(devices));
    else
        print_locked("[PID " + std::to_string(getpid()) + "] reserveblock failed: " + reason);

    SyncManager::write_unlock();
}

void RequestExecutor::execute_reserveany(const Request &req)
{
    if (req.args.size() != 6)
    {
        print_locked("reserveany failed: invalid argument");
        return;
    }
    int count;
    parse_int(req.args[0], count);
    int start, end;
    std::string reason;
    if (!parse_date_duration(req.args, 1, start, end, reason))
    {
        print_locked("reserveany failed: " + reason);
        return;
    }
    std::string user = req.args[5];

    SyncManager::write_lock();
    sleep(reserve_delay);

    if (count <= 0 || count > g_shm->device_count)
    {
        print_locked("[PID " + std::to_string(getpid()) + "] reserveany failed: invalid device");
        SyncManager::write_unlock();
        return;
    }

    std::vector<int> devices = ReservationManager::find_any_available_devices(count, start, end);
    if (devices.empty())
    {
        print_locked("[PID " + std::to_string(getpid()) + "] reserveany failed: device busy");
        SyncManager::write_unlock();
        return;
    }

    bool ok = ReservationManager::reserve_devices(devices, start, end, user, reason);
    if (ok)
        print_locked("[PID " + std::to_string(getpid()) + "] reserveany success: devices " + device_list_to_string(devices));
    else
        print_locked("[PID " + std::to_string(getpid()) + "] reserveany failed: " + reason);

    SyncManager::write_unlock();
}

void RequestExecutor::execute_cancel(const Request &req)
{
    if (req.args.size() != 6)
    {
        print_locked("cancel failed: invalid argument");
        return;
    }
    int device_id;
    parse_int(req.args[0], device_id);
    int start, end;
    std::string reason;
    if (!parse_date_duration(req.args, 1, start, end, reason))
    {
        print_locked("cancel failed: " + reason);
        return;
    }
    std::string user = req.args[5];

    SyncManager::write_lock();
    sleep(cancel_delay);

    int dev = DeviceManager::find_device_index(device_id);
    if (dev < 0)
    {
        print_locked("[PID " + std::to_string(getpid()) + "] cancel failed: invalid device");
        SyncManager::write_unlock();
        return;
    }

    std::vector<int> devices = {dev};
    bool ok = ReservationManager::cancel_devices(devices, start, end, user, reason);
    if (ok)
        print_locked("[PID " + std::to_string(getpid()) + "] cancel success: device " + std::to_string(device_id));
    else
        print_locked("[PID " + std::to_string(getpid()) + "] cancel failed: " + reason);

    SyncManager::write_unlock();
}

void RequestExecutor::execute_cancelblock(const Request &req)
{
    if (req.args.size() != 7)
    {
        print_locked("cancelblock failed: invalid argument");
        return;
    }
    int count, first_device_id;
    parse_int(req.args[0], count);
    parse_int(req.args[1], first_device_id);
    int start, end;
    std::string reason;
    if (!parse_date_duration(req.args, 2, start, end, reason))
    {
        print_locked("cancelblock failed: " + reason);
        return;
    }
    std::string user = req.args[6];

    SyncManager::write_lock();
    sleep(cancel_delay);

    std::vector<int> devices;
    if (!DeviceManager::build_block_devices(count, first_device_id, devices))
    {
        print_locked("[PID " + std::to_string(getpid()) + "] cancelblock failed: invalid device");
        SyncManager::write_unlock();
        return;
    }

    bool ok = ReservationManager::cancel_devices(devices, start, end, user, reason);
    if (ok)
        print_locked("[PID " + std::to_string(getpid()) + "] cancelblock success: devices " + device_list_to_string(devices));
    else
        print_locked("[PID " + std::to_string(getpid()) + "] cancelblock failed: " + reason);

    SyncManager::write_unlock();
}

void RequestExecutor::execute_cancelany(const Request &req)
{
    if (req.args.size() != 6)
    {
        print_locked("cancelany failed: invalid argument");
        return;
    }
    int count;
    parse_int(req.args[0], count);
    int start, end;
    std::string reason;
    if (!parse_date_duration(req.args, 1, start, end, reason))
    {
        print_locked("cancelany failed: " + reason);
        return;
    }
    std::string user = req.args[5];

    SyncManager::write_lock();
    sleep(cancel_delay);

    if (count <= 0 || count > g_shm->device_count)
    {
        print_locked("[PID " + std::to_string(getpid()) + "] cancelany failed: invalid device");
        SyncManager::write_unlock();
        return;
    }

    std::vector<int> devices = ReservationManager::find_any_cancel_devices(count, start, end, user);
    if (devices.empty())
    {
        print_locked("[PID " + std::to_string(getpid()) + "] cancelany failed: no such reservation");
        SyncManager::write_unlock();
        return;
    }

    bool ok = ReservationManager::cancel_devices(devices, start, end, user, reason);
    if (ok)
        print_locked("[PID " + std::to_string(getpid()) + "] cancelany success: devices " + device_list_to_string(devices));
    else
        print_locked("[PID " + std::to_string(getpid()) + "] cancelany failed: " + reason);

    SyncManager::write_unlock();
}

void RequestExecutor::execute_check(const Request &req)
{
    if (req.args.size() != 1)
    {
        print_locked("check failed: invalid argument");
        return;
    }
    ReservationManager::check_user(req.args[0], check_delay);
}

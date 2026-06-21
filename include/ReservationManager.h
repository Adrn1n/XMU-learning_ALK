#ifndef RESERVATION_MANAGER_H
#define RESERVATION_MANAGER_H

#include <vector>
#include <string>

class ReservationManager
{
private:
    static bool overlap(int l1, int r1, int l2, int r2);
    static void index_add(int rid);
    static void index_remove(int rid);

public:
    static int add_reservation(int user_id, int device_index, int start_day, int end_day);
    static void remove_reservation(int rid);
    static bool device_busy(int device_index, int start_day, int end_day);
    static bool can_reserve_all(const std::vector<int> &device_indices, int start_day, int end_day, std::string &reason);
    static bool reserve_devices(const std::vector<int> &device_indices, int start_day, int end_day, const std::string &user_name, std::string &reason);
    static int find_owned_covering_reservation(int user_id, int device_index, int start_day, int end_day);
    static bool has_other_user_reservation(int user_id, int device_index, int start_day, int end_day);
    static bool can_cancel_all(const std::vector<int> &device_indices, int start_day, int end_day, const std::string &user_name, std::vector<int> &reservation_ids, std::string &reason);
    static void cancel_by_reservation_ids(const std::vector<int> &reservation_ids, int start_day, int end_day);
    static bool cancel_devices(const std::vector<int> &device_indices, int start_day, int end_day, const std::string &user_name, std::string &reason);
    static std::vector<int> find_any_available_devices(int count, int start_day, int end_day);
    static std::vector<int> find_any_cancel_devices(int count, int start_day, int end_day, const std::string &user_name);
    static void check_user(const std::string &user_name, int delay);
    static void print_final_table();
};

#endif

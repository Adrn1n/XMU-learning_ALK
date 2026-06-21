#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <cstdint>

void bit_set(uint64_t *bits, int id);
void bit_clear(uint64_t *bits, int id);
bool bit_test(const uint64_t *bits, int id);

void print_locked(const std::string &s);

class DateUtil
{
public:
    static bool is_leap(int y);
    static int days_in_month(int y, int m);
    static bool valid_date(int y, int m, int d);
    static int to_day_index(int y, int m, int d);
    static void from_day_index(int idx, int &y, int &m, int &d);
    static std::string day_to_string(int idx);
};

#endif

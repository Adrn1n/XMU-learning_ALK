#include "Utils.h"
#include "SharedMemoryManager.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <semaphore.h>

void bit_set(uint64_t *bits, int id)
{
    bits[id / 64] |= (1ULL << (id % 64));
}

void bit_clear(uint64_t *bits, int id)
{
    bits[id / 64] &= ~(1ULL << (id % 64));
}

bool bit_test(const uint64_t *bits, int id)
{
    return (bits[id / 64] & (1ULL << (id % 64))) != 0;
}

void print_locked(const std::string &s)
{
    sem_wait(&g_shm->print_sem);
    std::cout << s << std::endl;
    std::cout.flush();
    sem_post(&g_shm->print_sem);
}

bool DateUtil::is_leap(int y)
{
    return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0);
}

int DateUtil::days_in_month(int y, int m)
{
    static int normal[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12)
        return -1;
    if (m == 2 && is_leap(y))
        return 29;
    return normal[m - 1];
}

bool DateUtil::valid_date(int y, int m, int d)
{
    if (y != 2026 && y != 2027)
        return false;
    int dim = days_in_month(y, m);
    return d >= 1 && d <= dim;
}

int DateUtil::to_day_index(int y, int m, int d)
{
    if (!valid_date(y, m, d))
        return -1;
    int day = 0;
    if (y == 2027)
        day += 365;
    for (int month = 1; month < m; ++month)
    {
        day += days_in_month(y, month);
    }
    day += d - 1;
    return day;
}

void DateUtil::from_day_index(int idx, int &y, int &m, int &d)
{
    y = 2026;
    if (idx >= 365)
    {
        y = 2027;
        idx -= 365;
    }
    m = 1;
    while (true)
    {
        int dim = days_in_month(y, m);
        if (idx < dim)
            break;
        idx -= dim;
        m++;
    }
    d = idx + 1;
}

std::string DateUtil::day_to_string(int idx)
{
    int y, m, d;
    from_day_index(idx, y, m, d);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return std::string(buf);
}

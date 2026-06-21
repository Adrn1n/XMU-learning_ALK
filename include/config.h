#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <semaphore.h>

static const int MAX_DEVICES = 128;
static const int MAX_USERS = 255;
static const int MAX_RESERVATIONS = 4096;
static const int MAX_NAME_LEN = 64;
static const int DAYS_2026_2027 = 365 + 365;

static const int DEVICE_WORDS = (MAX_DEVICES + 63) / 64;
static const int RES_WORDS = (MAX_RESERVATIONS + 63) / 64;

struct DeviceEntry
{
    int valid;
    int device_id;
};

struct UserEntry
{
    int valid;
    char name[MAX_NAME_LEN];
};

struct ReservationEntry
{
    int active;
    int user_id;
    int device_index;
    int start_day;
    int end_day;
};

struct SharedData
{
    sem_t resource_sem;
    sem_t read_mutex_sem;
    sem_t print_sem;

    int read_count;

    int device_count;
    DeviceEntry devices[MAX_DEVICES];

    int user_count;
    UserEntry users[MAX_USERS];

    ReservationEntry reservations[MAX_RESERVATIONS];

    uint64_t device_to_res[MAX_DEVICES][RES_WORDS];
    uint64_t day_to_res[DAYS_2026_2027][RES_WORDS];
    uint64_t user_to_res[MAX_USERS][RES_WORDS];
};

#endif

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <semaphore.h>
#include <fcntl.h> // 用于 macOS 下 sem_open 的 O_CREAT 和 O_EXCL

// macOS 与 Linux 信号量兼容定义
#ifdef __APPLE__
typedef sem_t *shm_sem_t;
#else
typedef sem_t shm_sem_t;
#endif

// 兼容的 wait 和 post 函数
inline int sem_wait_compat(shm_sem_t &sem)
{
#ifdef __APPLE__
    return sem_wait(sem);
#else
    return sem_wait(&sem);
#endif
}

inline int sem_post_compat(shm_sem_t &sem)
{
#ifdef __APPLE__
    return sem_post(sem);
#else
    return sem_post(&sem);
#endif
}

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
    // 使用兼容的信号量类型
    shm_sem_t resource_sem;
    shm_sem_t read_mutex_sem;
    shm_sem_t print_sem;

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

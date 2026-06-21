#ifndef SYNC_MANAGER_H
#define SYNC_MANAGER_H

class SyncManager
{
public:
    static void read_lock();
    static void read_unlock();
    static void write_lock();
    static void write_unlock();
};

#endif

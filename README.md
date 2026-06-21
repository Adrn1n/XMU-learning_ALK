# Linux Device Reservation System
## 1. Project Overview
This project implements a multi-process concurrent device reservation system under Linux.

The system simulates the operation of a laboratory equipment reservation management system. Each user is represented by one independent process. User processes concurrently execute reservation, cancellation, and query operations on shared reservation data.

The program uses the following Linux IPC mechanisms:
- **Shared memory**: stores devices, users, reservations, and inverted indexes.
- **POSIX semaphores**: synchronize concurrent access to shared memory.
- **Process creation with `fork()`**: each user request sequence is handled by one child process.

The implementation supports:
- Single-device reservation and cancellation
- Block reservation and cancellation for consecutive devices
- Automatic reservation and cancellation for arbitrary available devices
- Partial cancellation with interval splitting
- User reservation query
- Final reservation table output

The current version does not implement socket-based client/server communication. Instead, it reads a test file and simulates the server-side execution directly.

## 2. Directory Structure
```text
.
├── CMakeLists.txt
├── README.md
├── Report.md
├── include
│   ├── config.h
│   ├── DeviceManager.h
│   ├── InputParser.h
│   ├── RequestExecutor.h
│   ├── ReservationManager.h
│   ├── SharedMemoryManager.h
│   ├── SyncManager.h
│   ├── UserManager.h
│   └── Utils.h
├── src
│   ├── DeviceManager.cpp
│   ├── InputParser.cpp
│   ├── main.cpp
│   ├── RequestExecutor.cpp
│   ├── ReservationManager.cpp
│   ├── SharedMemoryManager.cpp
│   ├── SyncManager.cpp
│   ├── UserManager.cpp
│   └── Utils.cpp
├── assets
│   └── tests
│       └── test.txt
└── stress_test.py
```

## 3. Build Environment
### 3.1 Linux Test Environment
The program was tested on the following Linux environment:
```text
OS:
Linux raspberrypi 6.12.87+rpt-rpi-2712 #1 SMP PREEMPT Debian 1:6.12.87-1+rpt1~bookworm (2026-05-12) aarch64 GNU/Linux

Compiler:
g++ 12.2.0

CMake:
cmake version 3.25.1

Make:
GNU Make 4.3
```

Detailed compiler information:
```text
Target: aarch64-linux-gnu
Thread model: posix
gcc version 12.2.0 (Debian 12.2.0-14+deb12u1)
```

### 3.2 macOS Compatibility Environment
The program also contains compatibility code for macOS named semaphores.

Tested macOS environment:
```text
OS:
Darwin bogon 25.5.0 Darwin Kernel Version 25.5.0 arm64

Compiler:
Apple clang version 21.0.0

CMake:
cmake version 4.2.2

Make:
GNU Make 3.81
```

However, the course requirement targets Linux IPC. Therefore, the Linux environment is the primary execution environment.

## 4. Dependencies
The project only depends on standard system libraries.

Required tools:
- `g++`
- `cmake`
- `make`
- POSIX process and IPC support

No third-party C++ library is required.

For the optional stress test script:

- Python 3
- Standard Python modules only:
  - `argparse`
  - `datetime`
  - `os`
  - `random`
  - `re`
  - `sqlite3`
  - `subprocess`
  - `tempfile`
  - `time`

No Python third-party package is required.

## 5. Build Instructions
From the project root directory:
```bash
mkdir -p build
cd build
cmake ..
make
```

After successful compilation, the executable file is:
```text
build/test_server
```

## 6. Run Instructions
The program accepts one input file path as its argument.

Example:
```bash
./test_server ../assets/tests/test.txt
```

Or from the project root directory:
```bash
./build/test_server ./assets/tests/test.txt
```

## 7. Input File Format
The input file format is:
```text
n
device_id device_id ...
m
user
reserve reserve_time
cancel cancel_time
check check_time
request_1
request_2
...
end.
user
reserve reserve_time
cancel cancel_time
check check_time
request_1
request_2
...
end.
```

Where:
- `n` is the number of devices, not greater than 128.
- The next line gives all valid device IDs.
- `m` is the number of users, not greater than 255.
- Each `user ... end.` block represents one user process.
- `reserve_time`, `cancel_time`, and `check_time` are simulated operation delays in seconds.
- The delay is executed inside the corresponding critical section using `sleep()`.

Supported requests:
```text
reserve device_id year month day duration user_name
reserveblock device_count first_device_id year month day duration user_name
reserveany device_count year month day duration user_name

cancel device_id year month day duration user_name
cancelblock device_count first_device_id year month day duration user_name
cancelany device_count year month day duration user_name

check user_name
```

## 8. Example Input
```text
5
1 2 3 4 5
3
user
reserve 1
cancel 1
check 1
reserve 1 2026 1 1 5 Alice
check Alice
cancel 1 2026 1 2 2 Alice
check Alice
end.
user
reserve 1
cancel 1
check 1
reserve 1 2026 1 3 3 Bob
reserveany 2 2026 1 1 3 Bob
check Bob
end.
user
reserve 1
cancel 1
check 1
reserveblock 3 4 2026 2 1 2 Carol
check Carol
cancelany 1 2026 2 1 2 Carol
check Carol
end.
```

## 9. Example Output
A possible output is:
```text
[PID 422295] reserve success: device 1
[PID 422295] check Alice
  device_id=1 start=2026-01-01 end=2026-01-06

[PID 422297] reserveblock success: devices [4 5 1]
[PID 422297] check Carol
  device_id=4 start=2026-02-01 end=2026-02-03
  device_id=5 start=2026-02-01 end=2026-02-03
  device_id=1 start=2026-02-01 end=2026-02-03

[PID 422297] cancelany success: devices [1]
[PID 422297] check Carol
  device_id=4 start=2026-02-01 end=2026-02-03
  device_id=5 start=2026-02-01 end=2026-02-03

[PID 422295] cancel success: device 1
[PID 422295] check Alice
  device_id=1 start=2026-01-01 end=2026-01-02
  device_id=1 start=2026-01-04 end=2026-01-06

[PID 422296] reserve failed: device busy
[PID 422296] reserveany success: devices [2 3]
[PID 422296] check Bob
  device_id=2 start=2026-01-01 end=2026-01-04
  device_id=3 start=2026-01-01 end=2026-01-04

========== Final Reservation Table ==========
Device 1:
  user=Alice start=2026-01-01 end=2026-01-02
  user=Alice start=2026-01-04 end=2026-01-06
Device 2:
  user=Bob start=2026-01-01 end=2026-01-04
Device 3:
  user=Bob start=2026-01-01 end=2026-01-04
Device 4:
  user=Carol start=2026-02-01 end=2026-02-03
Device 5:
  user=Carol start=2026-02-01 end=2026-02-03
=============================================
```

Because multiple user processes run concurrently, the exact order of request output may change between executions.

## 10. Optional Stress Test
The project provides a Python stress test script:
```text
stress_test.py
```

Run example:
```bash
python3 stress_test.py ./build/test_server -n 100 --users 8 --devices 16 --ops 40 --seed 123
```

Run with sparse device IDs:
```bash
python3 stress_test.py ./build/test_server -n 100 --users 8 --devices 16 --ops 40 --sparse-devices --seed 123
```

Run with light delay simulation:
```bash
python3 stress_test.py ./build/test_server -n 20 --users 8 --devices 16 --ops 20 --delay-mode light --timeout 30
```

The stress test script checks the final reservation table using SQLite. It verifies:
1. All reservations use valid devices.
2. All reservation intervals are valid.
3. No two reservations overlap on the same device.
4. User names are valid.

## 11. Implementation Notes
### 11.1 IPC
The shared data area is allocated by:
```cpp
mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE,
     MAP_SHARED | MAP_ANONYMOUS, -1, 0)
```

On Linux, unnamed POSIX semaphores are initialized with:
```cpp
sem_init(&sem, 1, initial_value)
```

The second argument `1` means that the semaphore is shared among processes.

On macOS, named semaphores are used because process-shared unnamed semaphores are not supported in the same way.

### 11.2 Synchronization
The implementation uses a readers-writer synchronization scheme:
- Multiple `check` operations can read shared memory concurrently.
- Reservation and cancellation operations acquire the write lock.
- Write operations are mutually exclusive.
- While a write operation is executing, no reader is allowed to access shared data.

### 11.3 Atomicity
The following operations are atomic:
- `reserve`
- `reserveblock`
- `reserveany`
- `cancel`
- `cancelblock`
- `cancelany`

For block and any operations, the system first checks whether all required devices can be operated on. If any device fails, the entire operation fails and no partial modification is performed.

### 11.4 Date Representation
All dates are converted to day indexes.

```text
2026-01-01 -> 0
2026-12-31 -> 364
2027-01-01 -> 365
2027-12-31 -> 729
```

Reservations are stored as left-closed and right-open intervals:
```text
[start_day, end_day)
```

For example:
```text
reserve 1 2026 1 1 5 Alice
```

means:
```text
[2026-01-01, 2026-01-06)
```

The displayed end date is also the exclusive end date.

## 12. Limitations
1. The current version does not implement the socket-based client/server layer.
2. The maximum number of reservations is fixed by `MAX_RESERVATIONS`.
3. Shared memory uses fixed-size arrays instead of dynamic containers.
4. The device lookup is implemented by linear search.
5. The readers-writer lock is simple and may theoretically cause writer starvation under a heavy continuous read workload, although this is unlikely in the current test model.

## 13. Clean Build
To rebuild from scratch:
```bash
rm -rf build
mkdir build
cd build
cmake ..
make
```

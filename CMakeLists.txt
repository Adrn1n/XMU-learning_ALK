# cmake_minimum_required(VERSION 4.2)
cmake_minimum_required(VERSION 3.25.1)
project(XMU-learning_ALK-Final-Project)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

include_directories(include)

add_executable(test_server src/main.cpp src/InputParser.cpp src/SharedMemoryManager.cpp src/RequestExecutor.cpp src/Utils.cpp src/SyncManager.cpp src/DeviceManager.cpp src/ReservationManager.cpp src/UserManager.cpp)

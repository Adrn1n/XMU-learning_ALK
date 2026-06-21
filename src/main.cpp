#include "InputParser.h"
#include "SharedMemoryManager.h"
#include "RequestExecutor.h"
#include "ReservationManager.h"
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>

static void child_run(UserProgram program)
{
    RequestExecutor executor(program.reserve_delay, program.cancel_delay, program.check_delay);
    for (size_t i = 0; i < program.requests.size(); ++i)
    {
        executor.execute(program.requests[i]);
    }
    _exit(0);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " input.txt" << std::endl;
        return 1;
    }

    std::vector<int> device_ids;
    std::vector<UserProgram> programs;

    if (!InputParser::parse(argv[1], device_ids, programs))
        return 1;
    if (!SharedMemoryManager::init(device_ids))
        return 1;

    std::vector<pid_t> children;
    for (size_t i = 0; i < programs.size(); ++i)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            SharedMemoryManager::destroy();
            return 1;
        }
        if (pid == 0)
        {
            child_run(programs[i]);
        }
        else
        {
            children.push_back(pid);
        }
    }

    for (size_t i = 0; i < children.size(); ++i)
    {
        int status = 0;
        waitpid(children[i], &status, 0);
    }

    ReservationManager::print_final_table();
    SharedMemoryManager::destroy();

    return 0;
}

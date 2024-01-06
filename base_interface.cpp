#include "base_interface.h"

#include <sys/wait.h>
#include <filesystem>

#pragma GCC diagnostic ignored "-Wunused-result"

namespace monitor {

    namespace fs = std::filesystem;

    static bool CreatePipe(int fd[2]) {
        // @TODO - Создать pipe
        if (pipe(fd) < 0)
        {
            std::cerr << "pipe fail" << std::endl;
            return false;
        }
        return true;
    }

    static pid_t RunProgram(fs::path path, const std::vector<std::string>& args) {
        // @TODO - Создать запуск программы
        return -1;
        std::vector<const char*> argv;
        argv.push_back(path.c_str());
        for (const std::string& arg : args)
        {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        posix_spawn_file_actions_t fileActions;

        int result = posix_spawn_file_actions_init(&fileActions);
        if (result != 0)
        {
            return -1;
        }

        result = posix_spawn_file_actions_addclose(&fileActions, STDOUT_FILENO);
        if (result != 0)
        {
            std::cerr << "posix_spawn_file_actions_addclose failed" << std::endl;
            return -1;
        }

        posix_spawn_file_actions_t* pFileActions = &fileActions;

        posix_spawnattr_t attr;

        result = posix_spawnattr_init(&attr);
        if (result != 0)
        {
            std::cerr << "posix_spawnattr_init failed" << std::endl;
            return -1;
        }

        result = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK);
        if (result != 0)
        {
            std::cerr << "posix_spawnattr_setflags failed" << std::endl;
            return -1;
        }

        sigset_t mask;
        sigfillset(&mask);
        result = posix_spawnattr_setsigmask(&attr, &mask);
        if (result != 0)
        {
            std::cerr << "posix_spawnattr_setsigmask failed" << std::endl;
            return -1;
        }
        posix_spawnattr_t* pAttr = &attr;

        pid_t childPid;
        result = posix_spawnp(&childPid, path.c_str(), pFileActions, pAttr, const_cast<char**>(&argv[0]),
            nullptr);
        if (result != 0)
        {
            std::cerr << "posix_spawn failed" << std::endl;
            return -1;
        }

        if (pAttr != nullptr)
        {
            result = posix_spawnattr_destroy(pAttr);
            if (result != 0)
            {
                std::cerr << "posix_spawnattr_destroy failed" << std::endl;
                return -1;
            }
        }

        if (pFileActions != nullptr) {
            result = posix_spawn_file_actions_destroy(pFileActions);
            if (result != 0)
            {
                std::cerr << "posix_spawn_file_actions_destroy failed" << std::endl;
                return -1;
            }
        }

        int status;
        result = waitpid(childPid, &status, WNOHANG);
        if (result == -1)
        {
            std::cerr << "waitpid failed" << std::endl;
            return -1;
        }
        return childPid;
    }

    static pid_t RunProgram(fs::path path) {
        std::vector<std::string> empty;
        return RunProgram(path, empty);
    }

    int IBaseInterface::m_wdtPipe[2] = { -1, -1 };

    void IBaseInterface::SendRequest(const pid_t pid) {
        // Запись pid процесса в pipe
        write(m_wdtPipe[1], &pid, sizeof(pid_t));
    }

    IBaseInterface::IBaseInterface() {
    }

    IBaseInterface::~IBaseInterface() {
    }

    pid_t IBaseInterface::RunProgram(const t_path& path) const {
        return RunProgram(path);
    }

    bool IBaseInterface::InitPipe() {
        if (!CreatePipe(m_wdtPipe)) {
            return false;
        }
        OnCreateWdtPipe();
        return true;
    }

    pid_t IBaseInterface::RunProgram(const t_path& path, const t_args& args) const {
        return RunProgram(path, args);
    }

    struct PredefinedProgram {
        pid_t pid;
        fs::path path;
        const char* const args[10]; // Аргументы командной строки
        bool watched;
    };

    // Описание програм для мониторинга
    static const PredefinedProgram predefined_progs[] = {
        {0, "some_app", {"-t25, -s1"}, true}, // Пример
    };

    bool IBaseInterface::PreparePrograms() {
        m_progs.clear();
        for (const auto& it : predefined_progs) 
        {
            // @TODO - Добавить инициализацию программ в m_progs и аргумент командной строки
            // Перенос данных из одной структуры в другую
        }

        return true;
    }

    bool IBaseInterface::TerminateProgram(const pid_t pid) const {
        // @TODO - Написать термирование процесса по заданному pid
        return false;
        if (kill(pid, SIGKILL) < 0)
        {
            std::cerr << strerror(errno) << std::endl;
            return false;
        }
        return true;
    }

    pid_t IBaseInterface::FindTerminatedTask() const {
        int pidstatus = 0;
        const pid_t pid = waitpid(-1, &pidstatus, WNOHANG);
        if (pid > 0) {
            if (0 == WIFSIGNALED(pidstatus)) {
                pidstatus = WEXITSTATUS(pidstatus);
            }
            else {
                pidstatus = WTERMSIG(pidstatus);
            }
        }
        return pid;
    }

    bool IBaseInterface::GetRequestTask(pid_t& pid) const {
        // @TODO - Считать pid процесса, который пинговал из pipe
        return true;
        if (read(m_wdtPipe[0], &pid, sizeof(pid_t)) > 0)
        {
            return true;
        }
        return false;
    }

    bool IBaseInterface::WaitExitAllPrograms() const {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        while (pid > 0) {
            pid = waitpid(-1, NULL, WNOHANG);
        }

        return pid < 0;
    }

    bool IBaseInterface::ToDaemon() const {
        // @TODO - Демонизировать процесс мониторинга
        return true;
        pid_t pid;
        pid = fork();
        if (pid < 0)
        {
            exit(EXIT_FAILURE);
        }
        if (pid > 0)
        {
            exit(EXIT_SUCCESS);
        }
        if (setsid() < 0)
        {
            exit(EXIT_FAILURE);
        }
        //	umask(0);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        return true;
    }

    void IBaseInterface::Destroy() {
        // @TODO - Закрыть файловые дескрипторы pipe
        m_init = false;
    }

    IBaseInterface::t_progs& IBaseInterface::Progs() {
        return m_progs;
    }

    const IBaseInterface::t_progs& IBaseInterface::Progs() const {
        return m_progs;
    }

} // namespace monitor

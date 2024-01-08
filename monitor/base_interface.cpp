#include "base_interface.h"

#include <sys/wait.h>

#pragma GCC diagnostic ignored "-Wunused-result"

namespace monitor
{

namespace fs = std::filesystem;

static bool wdt_create_pipe([[maybe_unused]] int fd[2])
{
    // @TODO - создать pipe
    if (pipe(fd) == -1)
    {
        return false;
    }
    return true;
}

static pid_t run_program([[maybe_unused]] fs::path path, [[maybe_unused]] const std::vector<std::string>& args)
{
    // @TODO - написать запуск программы
    pid_t pid = fork();
    if (pid < 0) 
    {
        return -1;
    }
    else if (pid == 0) 
    {
        std::vector<char*> c_args;
        for (const auto& arg : args) 
        {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);
        execv(path.c_str(), c_args.data());
        exit(0);
    }
    return pid;
}

static pid_t run_program(fs::path path)
{
    std::vector<std::string> empty;
    return run_program(path, empty);
}

//static
int IBaseInterface::m_wdtPipe[2] = { -1, -1 };

//static
void IBaseInterface::send_request(const pid_t pid)
{
    // запись pid процесса в пайп
    write(m_wdtPipe[1], &pid, sizeof(pid_t));
}

/**
 * Конструктор
 */
IBaseInterface::IBaseInterface()
{
}

IBaseInterface::~IBaseInterface()
{
}

pid_t IBaseInterface::RunProgram(const t_path& path) const
{
    return run_program(path);
}

bool IBaseInterface::InitPipe()
{
    if (!wdt_create_pipe(m_wdtPipe))
    {
        return false;
    }
    OnCreateWdtPipe();
    return true;
}

pid_t IBaseInterface::RunProgram(const t_path& path, const t_args& args) const
{
    return run_program(path, args);
}

struct t_predefined_program
{
    pid_t pid;
    const fs::path path;
    const char *const args[10]; // аргументы командной строки
    bool watched;
};

/** Описание предопределенных программ для мониторинга */
static const t_predefined_program predefined_progs[] =
{
    {0, "some_app", {"-t25, -s1"}, true}, // пример
};

bool IBaseInterface::PreparePrograms()
{
    m_progs.clear();
    for ([[maybe_unused]] auto& it : predefined_progs)
    {
        // @TODO - добавить инициализацию программ в m_progs, в том числе аргумент командной строки
        // здесь по сути просто переложить из одной структуры в другую
        t_program prog;
        prog.pid = it.pid;
        prog.path = it.path;
        for (int i = 0; it.args[i] != nullptr; ++i)
        {
            prog.args.push_back(it.args[i]);
        }
        m_progs.push_back(prog);
    }

    return true;
}

bool IBaseInterface::TerminateProgram([[maybe_unused]] const pid_t pid) const
{
    // @TODO - написать терминирование процесса по заданному pid
      if (kill(pid, SIGTERM) == 0) 
    {
        return true;
    }
      return false;
}

pid_t IBaseInterface::FindTerminatedTask() const
{
    int pidstatus = 0;
    const pid_t pid = waitpid(-1, &pidstatus, WNOHANG);
    if (pid > 0)
    {
        if (0 == WIFSIGNALED(pidstatus))
        {
            pidstatus = WEXITSTATUS(pidstatus);
        }
        else
        {
            pidstatus = WTERMSIG(pidstatus);
        }
    }
    return pid;
}

bool IBaseInterface::GetRequestTask([[maybe_unused]] pid_t& pid) const
{
    // @TODO - считать пид процесса, который пинговал из пайпа
    ssize_t bytes_read = read(m_wdtPipe[0], &pid, sizeof(pid_t));
    if (bytes_read == sizeof(pid_t)) 
    {
        return true;
    }
    return false;
}

bool IBaseInterface::WaitExitAllPrograms() const
{
    pid_t pid = waitpid(-1, NULL, WNOHANG);
    while (pid > 0)
    {
        pid = waitpid(-1, NULL, WNOHANG);
    }

    return pid < 0;
}

bool IBaseInterface::ToDaemon() const
{
    // @TODO - демонизировать процесс мониторинга
    if (daemon(1, 0) == -1) 
    {
        return false;
    }
    return true;
}

void IBaseInterface::Destroy()
{
    // @TODO - закрыть файловые дескрипторы пайпа
    close(m_wdtPipe[0]);
    close(m_wdtPipe[1]);
    m_init = false;
}

IBaseInterface::t_progs& IBaseInterface::Progs()
{
    return m_progs;
}

/**
 * Получить список наблюдаемых программ
 * @return список наблюдаемых программ
 */
const IBaseInterface::t_progs& IBaseInterface::Progs() const
{
    return m_progs;
}

} // namespace monitor

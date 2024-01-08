#include "base_interface.h"

#include <sys/wait.h>

#pragma GCC diagnostic ignored "-Wunused-result"

namespace monitor
{

namespace fs = std::filesystem;

static bool wdt_create_pipe( int fd[2])
{
    // @TODO - создать pipe
    return pipe(fd) == 0;
}

static pid_t run_program(fs::path path, const std::vector<std::string>& args)
{
    // @TODO - написать запуск программы
    if (pid == -1) {
    return -1;
    }
    else if (pid > 0) {
        return pid;
    } 
    else { 
        std::vector<const char*> cargs;
        cargs.reserve(args.size() + 2);
        cargs.push_back(path.c_str());
        for (const auto& arg : args) {
            cargs.push_back(arg.c_str());
        }
        cargs.push_back(nullptr);
        execvp(path.c_str(), const_cast<char *const *>(cargs.data()));
        _exit(EXIT_FAILURE);
    }
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
    for (const auto& it : predefined_progs)
    {
        // @TODO - добавить инициализацию программ в m_progs, в том числе аргумент командной строки
        // здесь по сути просто переложить из одной структуры в другую
        std::vector<std::string> args;
        for (int i = 0; it.args[i]; ++i) {
            args.emplace_back(it.args[i]);
        }
        auto& prog = m_progs.emplace_back();
        prog.pid = 0;
        prog.path = it.path;
        prog.args = std::move(args);
        prog.watched = it.watched;
    }
    return true;
}

bool IBaseInterface::TerminateProgram(const pid_t pid) const
{
    // @TODO - написать терминирование процесса по заданному pid
    return kill(pid, SIGTERM) == 0;
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

bool IBaseInterface::GetRequestTask(pid_t& pid) const
{
    // @TODO - считать пид процесса, который пинговал из пайпа
    return read(m_wdtPipe[0], &pid, sizeof(pid_t)) > 0;
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
    if (daemon(1, 0) == -1) {
        return false;
    }
    return true;
}

void IBaseInterface::Destroy()
{
    // @TODO - закрыть файловые дескрипторы пайпа
    int n = fcntl(pipedes[0], F_GETFL);
    fcntl(pipedes[0], F_SETFL, n | O_NONBLOCK);
    n = fcntl(pipedes[1], F_GETFL);
    fcntl(pipedes[1], F_SETFL, n | O_NONBLOCK);
    return true;
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

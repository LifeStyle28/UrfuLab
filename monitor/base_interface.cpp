#include "base_interface.h"

#include <sys/wait.h>

#pragma GCC diagnostic ignored "-Wunused-result"

namespace monitor
{

namespace fs = std::filesystem;

static bool wdt_create_pipe([[maybe_unused]] int fd[2])
{
    // @TODO - создание pipe
    if (pipe(fd) < 0)
    {
        std::cerr << "Failed to create pipe" << std::endl;
        return false;
    }

    if (pid == -1)
    {
        std::cerr << "error when forked, errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    if (pid == 0)
    {
        const std::string str = "Hello, world!";
        std::cout << "I'm a child process" << std::endl;
        write(fd[1], str.c_str(), str.size());
        std::cout << "Write, str = " << str << std::endl;
    }
    else if (waitpid(pid, nullptr, 0))
    {
        char buf[64];
        std::cout << "I'm a parent process" << std::endl;
        if (read(fd[0], buf, sizeof(buf)) > 0)
        {
            std::cout << "Read, buf = " << buf << std::endl;
        }
    }
    int n = fcntl(pipedes[0], F_GETFL);
    fcntl(pipedes[0], F_SETFL, n | O_NONBLOCK);
    n = fcntl(pipedes[1], F_GETFL);
    fcntl(pipedes[1], F_SETFL, n | O_NONBLOCK);
    return true;
}

static pid_t run_program([[maybe_unused]] fs::path path, [[maybe_unused]] const std::vector<std::string>& args)
{
    // @TODO - запуск программы
    // подготавливаем аргументы для запуска
    std::vector<const char*> argv;
    argv.push_back(path.c_str());
    for (const std::string& arg : args)
    {
        argv.push_back(arg.c_str());
    }
    argv.push_back(NULL);

    posix_spawn_file_actions_t fileActions;

    int result = posix_spawn_file_actions_init(&fileActions);
    if (result != 0)
    {
        std::cerr << "posix_spawn_file_actions_init failed" << std::endl;
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

    if (pAttr != NULL)
    {
        result = posix_spawnattr_destroy(pAttr);
        if (result != 0)
        {
            std::cerr << "posix_spawnattr_destroy failed" << std::endl;
            return -1;
        }
    }

    if (pFileActions != NULL) {
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
        // @TODO - инициализация программ в m_progs, в том числе аргумент командной строки
        std::string program_name = it.first;
        std::string program_argument = it.second;

        // Добавляем аргумент командной строки в программу
        if (argc > 1) {
            program_argument += " " + std::string(argv[1]);
        }

        // Инициализируем программу и добавляем ее в m_progs
        t_prog prog;
        prog.pid = 0;
        prog.path = it.path;
        foreach(const char* const& arg, it.args)
        {
            if (arg)
            {
                prog.args.push_back(arg);
            }
        }
        prog.watched = it.watched;
        m_progs.push_back(prog);
    }

    return true;
}

bool IBaseInterface::TerminateProgram([[maybe_unused]] const pid_t pid) const
{
    // @TODO - терминирование процесса по заданному pid
    //Пробуем отправить SIGKILL сигнал
    if (kill(pid, SIGKILL) == 0) {
        return true;
    }
    // Если и отправка SIGKILL не удалась, значит процесс уже завершен или не существует
   
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
    // @TODO - считывание пид процесса, который пинговал из пайпа
    return sizeof(pid_t) == read(m_wdtPipe[0], &pid, sizeof(pid_t));
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
    // @TODO - демонизирование процесса мониторинга
    pid_t pid = fork();

    if (pid < 0) {
        // Ошибка при создании процесса
        std::cerr << "Ошибка при создании процесса" << std::endl;
        return false;
    }

    if (pid > 0) {
        // Завершаем родительский процесс
        exit(EXIT_SUCCESS);
    }

    // Дочерний процесс

    // Создаем новый SID для дочернего процесса
    if (setsid() < 0) {
        // Ошибка при создании нового SID
        std::cerr << "Ошибка при создании нового SID" << std::endl;
        return false;
    }
    // Закрываем стандартные потоки ввода/вывода/ошибок
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return true;
}

void IBaseInterface::Destroy()
{
    // @TODO - закрытие файловых дескрипторов пайпа
    if (m_wdtPipe[0] != -1)
    {
        close(m_wdtPipe[0]);
    }
    if (m_wdtPipe[1] != -1)
    {
        close(m_wdtPipe[1]);
    }
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

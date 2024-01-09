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
		Null);
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
    for (auto& it : predefined_progs)
    {
        // @TODO - добавить инициализацию программ в m_progs, в том числе аргумент командной строки
        // здесь по сути просто переложить из одной структуры в другую
        t_prog prog;
        prog.pid = it.pid;
        prog.path = it.path;
        prog.args = it.args;
        prog.watched = it.watched;
        m_progs.push_back(prog);
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

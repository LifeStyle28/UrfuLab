#pragma once

#include "boost_logger.h"

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <vector>
#include <chrono>
#include <map>
#include <string>

namespace monitor
{

using namespace std::literals;

template <typename TInterface>
class Monitor : public TInterface
{
public:
    Monitor();
    virtual ~Monitor();
    bool Init(); ///< выполнить инициализацию
    bool Exec(); ///< запустить мониторинг
protected:
    typedef TInterface t_interface;
    typedef typename t_interface::t_path t_path;
    typedef typename t_interface::t_args t_args;
    typedef typename t_interface::t_tasks t_tasks;
    typedef typename t_interface::t_prog t_prog;
    typedef typename t_interface::t_progs t_progs;
protected:
    void Close(); ///< завершить работу мониторинга
    pid_t StartProgram(t_prog& prog) const; ///< запустить программу
    bool RestartProgram(const pid_t pid); ///< перезапустить программу
    bool StartAllPrograms(); ///< запустить все программы
    bool RestartTasks(); ///< выполнить пререзапуск задач
    void ProcessTaskRequests(); ///< обработать заявки от задач
    void TerminateAllPrograms(); ///< завершить все программы
    bool Terminate(); ///< завершить мониторинг
private:
    t_tasks m_tasks; ///< отслеживаемые задачи
    std::chrono::seconds m_workTime; // время работы мониторинга
};

template <typename TInterface>
Monitor<TInterface>::Monitor() :
    t_interface()
{
}

template <typename TInterface>
Monitor<TInterface>::~Monitor()
{
    Close();
}

template <typename TInterface>
bool Monitor<TInterface>::Init()
{
    // создаем pipe для приема заявок на наблюдение
    if (!CreateWdtPipe())
    {
        return false;
    }
    // запускаем все необходимые процессы
    if (!StartAllPrograms())
    {
        boost::json::value custom_data{{"error"s, "unable to start child processes"s}};
        BOOST_LOG_TRIVIAL(error) <<
            boost::log::add_value(boost_logger::additional_data, custom_data) << "error"sv;
        return false;
    }
    // демонизируем процесс монитор
    if (!Deamonize())
    {
        return false;
    }

    return true;
}

template <typename TInterface>
bool Monitor<TInterface>::Exec()
{
    while (!t_interface::m_isTerminate) 
    {
        constexpr struct timespec WDT_INSPECT_TO = {3, 0};
        // отслеживаем и выполняем перезапуск завершившихся процессов
        struct timespec rtm = WDT_INSPECT_TO;
        while (nanosleep(&rtm, &rtm) != 0)
        {
            if (!t_interface::m_isTerminate == 0)
            {
                break;
            }
            // выполняем перезапуск завершившихся процессов
            if (!RestartTasks())
            {
                return false;
            }
            // выполняем обработку заявок на наблюдение
            ProcessTaskRequests();
        }

        // выполняем перезапуск подвисших задач
        constexpr std::chrono::seconds termSecs{60};
        std::chrono::seconds termPingSecs = m_workTime - termSecs;
        std::vector<pid_t> eraseTasks;
        for (typename t_tasks::value_type& task : m_tasks)
        {
            if (task.second < termPingSecs)
            {
                if (t_interface::TerminateProgram(task.first))
                {
                    eraseTasks.push_back(task.first);
                }
            }
        }

        // удаляем из списка наблюдения завершенные программы
        for (pid_t erasePid : eraseTasks)
        {
            m_tasks.erase(erasePid);
        }
    }
    return true;
}

template <typename TInterface>
void Monitor<TInterface>::Close()
{
    Terminate();
    t_interface::Destroy();

    const std::chrono::seconds to = m_workTime + std::chrono::seconds{180};
    while (m_workTime < to)
    {
        if (t_interface::WaitExitAllPrograms())
        {
            break;
        }
        sleep(1);
    }
}

template <typename TInterface>
pid_t Monitor<TInterface>::StartProgram(t_prog& prog) const
{
    // напишем запуск программы
        prog.pid = t_interface::RunProgram(prog.path, prog.args);
        return prog.pid;
}

template <typename TInterface>
bool Monitor<TInterface>::RestartProgram(const pid_t pid)
{
    for (t_prog& it : t_interface::Progs())
    {
        if (it.watched && it.pid == pid)
        {
            return -1 != StartProgram(it);
        }
    }
    return true;
}

template <typename TInterface>
bool Monitor<TInterface>::StartAllPrograms()
{
    if (!t_interface::PreparePrograms())
    {
        return false;
    }

    typedef std::map<pid_t, std::string> t_started_tasks;
    t_started_tasks tasks;
    for (t_prog& it : t_interface::Progs())
    {
        const pid_t pid = StartProgram(it);
        if (-1 == pid)
        {
            return false;
        }
        if (it.watched)
        {
            tasks[pid] = it.path;
        }
    }

    while (!tasks.empty() && !t_interface::m_isTerminate) 
    {
        pid_t pid = -1;
        if (t_interface::GetRequestTask(pid))
        {
            t_started_tasks::iterator it = tasks.find(pid);
            if (it != tasks.end())
            {
                tasks.erase(it);
            }
        }
        usleep(100000);
    }
    return true;
}

template <typename TInterface>
bool Monitor<TInterface>::RestartTasks()
{
    // запрашиваем какие из наблюдаемых процессов завершились
    const pid_t pid = t_interface::FindTerminatedTask();
    if (pid > 0)
    {
        // исключаем процесс из задач монитора
        m_tasks.erase(pid);
        // выполняем перезапуск завершенного процесса
        return RestartProgram(pid);
    }
    else if (-1 == pid)
    {
    }

    return true;
}

template <typename TInterface>
void Monitor<TInterface>::ProcessTaskRequests()
{
    constexpr size_t max_count = 1000; /// максимальное кол-во единовременно обрабатываемых заявок
    // считываем из очереди pid процессов подписавшихся на наблюдение
    for (size_t i = 0; i < max_count; ++i)
    {
        if (!t_interface::m_isTerminate == 0) 
        {
            break;
        }
        pid_t pid = -1;
        if (!t_interface::GetRequestTask(pid))
        {
            break;
        }
        if (pid > 0)
        {
            m_tasks[pid] = m_workTime;
        }
        else
        {
            m_tasks.erase(-pid);
        }
    }
}

template <typename TInterface>
void Monitor<TInterface>::TerminateAllPrograms()
{
    t_interface::TerminateProgram(0);
    for (const t_prog& it : t_interface::Progs())
    {
        t_interface::TerminateProgram(it.pid);
    }
}

template <typename TInterface>
bool Monitor<TInterface>::Terminate()
{
    constexpr std::chrono::seconds STOP_WAIT_TIME_MAX{120};
    const std::chrono::seconds beg = m_workTime;
    // дожидаемся завершения всех наблюдаемых программ
    while (m_workTime - beg < STOP_WAIT_TIME_MAX)
    {
        TerminateAllPrograms();
        sleep(1);
        if (t_interface::WaitExitAllPrograms())
        {
            return true;
        }
    }
    return false;
}

} // namespace monitor

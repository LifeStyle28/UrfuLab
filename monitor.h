#pragma once

#include "boost_logger.h"
#include <iostream>

#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <csignal>

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
        std::chrono::seconds WorkingTime();
    private:
        t_tasks m_tasks; ///< отслеживаемые задачи
        std::chrono::time_point<std::chrono::steady_clock> m_startTime;
    };

    template <typename TInterface>
  Monitor<TInterface>::Monitor() : t_interface(), m_startTime(std::chrono::steady_clock::now()) 
    {

    }

    template <typename TInterface>
    Monitor<TInterface>::~Monitor()
    {
        Close();
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Init() {
        if (!t_interface::InitPipe()) {
            BOOST_LOG_TRIVIAL(error) << "Error: Failed to create a pipe!";
            return false;
        }

        BOOST_LOG_TRIVIAL(info) << "Initialization successful.";

        if (!t_interface::ToDaemon()) {
            BOOST_LOG_TRIVIAL(error) << "Error: Failed to create daemon!";
            return false;
        }

        if (!StartAllPrograms()) {
            BOOST_LOG_TRIVIAL(error) << "Error: Failed to start programs!";
            return false;
        }

        return true;
    }

    template <typename TInterface>
    std::chrono::seconds Monitor<TInterface>::WorkingTime() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_startTime);
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Exec() {
        while (!t_interface::m_isTerminate) {
            for (const auto& [pid, time] : m_tasks) {
                BOOST_LOG_TRIVIAL(info) << "Process status (pid, ping_time): " << pid << ", " << time.count();
            }

            ProcessTaskRequests();

            constexpr std::chrono::seconds termSecs{ 60 };
            std::chrono::seconds termPingSecs = WorkingTime() - termSecs;
            std::vector<pid_t> eraseTasks;

            for (auto& [pid, time] : m_tasks) {
                if (time < termPingSecs) {
                    BOOST_LOG_TRIVIAL(info) << "Restarting freeze program with PID: " << pid;
                    if (t_interface::TerminateProgram(pid)) {
                        eraseTasks.push_back(pid);
                    }
                }
            }

            for (pid_t erasePid : eraseTasks) {
                m_tasks.erase(erasePid);

                if (!RestartProgram(erasePid)) {
                    BOOST_LOG_TRIVIAL(error) << "Error: Failed to restart freeze program with PID: " << erasePid;
                    return false;
                }
            }

            if (!RestartTasks()) {
                BOOST_LOG_TRIVIAL(error) << "Error: Failed to restart programs!";
                return false;
            }
        }

        return true;
    }
    template <typename TInterface>
    std::chrono::seconds Monitor<TInterface>::WorkingTime()
    {
        return std::chrono::duration_cast<std::chrono::seconds>
            (std::chrono::steady_clock::now() - m_startTime);
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Exec()
    {
        while (!t_interface::m_isTerminate)
        {
            for (typename t_tasks::value_type& task : m_tasks)
            {
                boost::json::value custom_data{ {"Process status(pid, ping_time)", task.first, task.second.count()} };
                BOOST_LOG_TRIVIAL(info) << boost::log::add_value(boost_logger::additional_data, custom_data)
                    << "Process status!"sv;
            }

            ProcessTaskRequests();

            // выполняем перезапуск подвисших задач
            constexpr std::chrono::seconds termSecs{ 60 };
            std::chrono::seconds termPingSecs = WorkingTime() - termSecs;
            std::vector<pid_t> eraseTasks;
            for (typename t_tasks::value_type& task : m_tasks)
            {
                if (task.second < termPingSecs)
                {
                    boost::json::value custom_data{ {"Program PID"s, task.first} };
                    BOOST_LOG_TRIVIAL(info) << boost::log::add_value(boost_logger::additional_data, custom_data)
                        << "Restart freeze program!"sv;
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
                if (!RestartProgram(erasePid))
                {
                    boost::json::value custom_data{ {"Program PID"s, erasePid} };
                    BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data)
                        << "Failed restart freeze program!"sv;
                    return false;
                }
            }
            if (!RestartTasks())
            {
                boost::json::value custom_data{ {} };
                BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data)
                    << "Failed restart programs!"sv;
                return false;
            }
        }
        return true;
    }

    template <typename TInterface>
    void Monitor<TInterface>::Close()
    {
        Terminate();
        t_interface::Destroy();
        const std::chrono::seconds to = WorkingTime() + std::chrono::seconds{ 180 };
        while (WorkingTime() < to)
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
                boost::json::value custom_data{ {"Restarted program PID", it.pid} };
                BOOST_LOG_TRIVIAL(info) << boost::log::add_value(boost_logger::additional_data, custom_data)
                    << "Restarted program!"sv;
                return -1 != StartProgram(it);;
            }
        }
        return true;
    }

    template <typename TInterface>
    bool Monitor<TInterface>::StartAllPrograms()
    {
        if (!t_interface::PreparePrograms())
        {
            boost::json::value custom_data{ {} };
            BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data)
                << "Failed to prepare programs!"sv;
            return false;
        }

        typedef std::map<pid_t, std::string> t_started_tasks;
        t_started_tasks tasks;
        for (t_prog& it : t_interface::Progs())
        {
            const pid_t pid = StartProgram(it);
            if (-1 == pid)
            {
                boost::json::value custom_data{ {"Program name"s, it.path.string()} };
                BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data)
                    << "Failed to start program!"sv;
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
            if (t_interface::m_isTerminate)
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
                m_tasks[pid] = WorkingTime();
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
        for (const t_prog& it : t_interface::Progs())
        {
            t_interface::TerminateProgram(it.pid);
            boost::json::value custom_data{ {"Terminated program"s, it.pid} };
            BOOST_LOG_TRIVIAL(info) << boost::log::add_value(boost_logger::additional_data, custom_data)
                << "Terminated program!"sv;
        }
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Terminate()
    {
        constexpr std::chrono::seconds STOP_WAIT_TIME_MAX{ 120 };
        const std::chrono::seconds beg = WorkingTime();
        // дожидаемся завершения всех наблюдаемых программ
        while (WorkingTime() - beg < STOP_WAIT_TIME_MAX)
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

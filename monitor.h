#pragma once

#include "boost_logger.h"

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <vector>
#include <chrono>

namespace monitor {

    using namespace std::literals;

    template <typename TInterface>
    class Monitor : public TInterface {
    public:
        Monitor();
        virtual ~Monitor();

        bool Init();
        bool Exec(); 

    protected:
        typedef TInterface t_interface;
        typedef typename t_interface::t_path t_path;
        typedef typename t_interface::t_args t_args;
        typedef typename t_interface::t_tasks t_tasks;
        typedef typename t_interface::t_prog t_prog;
        typedef typename t_interface::t_progs t_progs;

    protected:
        void Close(); 
        pid_t StartProgram(t_prog& prog) const;
        bool RestartProgram(const pid_t pid); 
        bool StartAllPrograms();
        bool RestartTasks(); 
        void ProcessTaskRequests();
        void TerminateAllPrograms();
        bool Terminate(); 

    private:
        t_tasks m_tasks;
        std::chrono::seconds m_workTime; // @TODO 
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
        // @TODO
        if (!StartAllPrograms()) {
            boost::json::value custom_data{ {"error"s, "Unable to start child processes"s} };
            BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data) << "error"sv;
            return false;
        }
        // @TODO

        return true;
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Exec()
    {
        while (/*!is_terminated()*/1) // @TODO  
        {
            constexpr struct timespec WDT_INSPECT_TO = { 3, 0 };
            struct timespec rtm = WDT_INSPECT_TO;
            while (nanosleep(&rtm, &rtm) != 0)
            {
                if (/*is_terminated()*/0) {
                    break;
                }
                if (!RestartTasks()) {
                    return false;
                }
                ProcessTaskRequests();
            }

            constexpr std::chrono::seconds termSecs{ 60 };
            std::chrono::seconds termPingSecs = m_workTime - termSecs;
            std::vector<pid_t> eraseTasks;
            for (auto& task : m_tasks)
            {
                if (task.second < termPingSecs)
                {
                    if (t_interface::TerminateProgram(task.first))
                    {
                        eraseTasks.push_back(task.first);
                    }
                }
            }

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

        const std::chrono::seconds to = m_workTime + std::chrono::seconds{ 180 };
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
        // @TODO
        return 0;
    }

    template <typename TInterface>
    bool Monitor<TInterface>::RestartProgram(const pid_t pid)
    {
        for (auto& it : t_interface::Progs())
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
        for (auto& it : t_interface::Progs())
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

        while (!tasks.empty() /*&& !is_terminated()*/) // @TODO
        {
            pid_t pid = -1;
            if (t_interface::GetRequestTask(pid))
            {
                auto it = tasks.find(pid);
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
        const pid_t pid = t_interface::FindTerminatedTask();
        if (pid > 0)
        {
            m_tasks.erase(pid);
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
        constexpr size_t max_count = 1000;
        for (size_t i = 0; i < max_count; ++i)
        {
            if (/*is_terminated()*/0) // @TODO - 
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
        t_interface::TerminateProgram(0);
        for (const auto& it : t_interface::Progs())
        {
            t_interface::TerminateProgram(it.pid);
        }
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Terminate()
    {
        constexpr std::chrono::seconds STOP_WAIT_TIME_MAX{ 120 };
        const std::chrono::seconds beg = m_workTime;
        while (m_workTime - beg < STOP_WAIT_TIME_MAX)
        {
            TerminateAllPrograms();
            sleep(1);
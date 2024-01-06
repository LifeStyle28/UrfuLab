#pragma once

#include "boost_logger.h"

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <vector>
#include <chrono>
#include <map>
#include <string>

namespace monitor {

    using namespace std::literals;

    template <typename TInterface>
    class Monitor : public TInterface {
    public:
        Monitor();
        virtual ~Monitor();

        bool Init(); ///< Выполнение инициализации
        bool Exec(); ///< Запуск мониторинга

    protected:
        typedef TInterface t_interface;
        typedef typename t_interface::t_path t_path;
        typedef typename t_interface::t_args t_args;
        typedef typename t_interface::t_tasks t_tasks;
        typedef typename t_interface::t_prog t_prog;
        typedef typename t_interface::t_progs t_progs;

    protected:
        void Close(); ///< Terminate monitoring
        pid_t StartProgram(t_prog& prog) const; ///< Запуск программы
        bool RestartProgram(const pid_t pid); ///< Перезапуск программы
        bool StartAllPrograms(); ///< Запуск всех программ
        bool RestartTasks(); ///< Перезапуск задач
        void ProcessTaskRequests(); ///< Обработка запросов задач
        void TerminateAllPrograms(); ///< Завершение всех программ
        bool Terminate(); ///< Завершение мониторинга

    private:
        t_tasks m_tasks; ///< отслеживаемые задачи
        std::chrono::seconds m_workTime; // @TODO - вычисление
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
        // @TODO - создаем pipe для приема заявок на мониторинг
        // Запуск всех необходимых процессов
        if (!StartAllPrograms()) {
            boost::json::value custom_data{ {"error"s, "Unable to start child processes"s} };
            BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data) << "error"sv;
            return false;
        }
        // @TODO - Демонизируем процесс монитора

        return true;
    }

    template <typename TInterface>
    bool Monitor<TInterface>::Exec()
    {
        while (/*!is_terminated()*/1) // @TODO - проверка не терминирован ли процесс
        {
            constexpr struct timespec WDT_INSPECT_TO = { 3, 0 };
            // Мониторинг и перезапуск завершенных процессов
            struct timespec rtm = WDT_INSPECT_TO;
            while (nanosleep(&rtm, &rtm) != 0)
            {
                if (/*is_terminated()*/0) {
                    break;
                }
                // Перезапуск зависших процессов
                if (!RestartTasks()) {
                    return false;
                }
                // Обработка запросов на наблюдение
                ProcessTaskRequests();
            }

            // Перезапуск зависших задач
            constexpr std::chrono::seconds termSecs{ 60 };
            std::chrono::seconds termPingSecs = WorkingTime() - termSecs;
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

            // Удаление завершенных программ из списка наблюдения
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
        // @TODO - Запуск программы
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

        while (!tasks.empty() /*&& !is_terminated()*/) // @TODO - Расскоментить, когда функция будет написана
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
        // Запрос информации о завершенных процессах
        const pid_t pid = t_interface::FindTerminatedTask();
        if (pid > 0)
        {
            // Исключение процесса из задач монитора
            m_tasks.erase(pid);
            // Перезапуск процесса
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
        constexpr size_t max_count = 1000; /// Максимальное количество одновременно обрабатываемых запросов
        // Считывание pid процессов из очереди, подписавшихся на мониторинг
        for (size_t i = 0; i < max_count; ++i)
        {
            if (/*is_terminated()*/0) // @TODO - Раскомментить, когда функция будет реализована
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
        const std::chrono::seconds beg = WorkingTime();
        // Дожидаемся завершения работы всех отслеживаемых программ
        while (WorkingTime() - beg < STOP_WAIT_TIME_MAX)
        {
            TerminateAllPrograms();
            sleep(1);

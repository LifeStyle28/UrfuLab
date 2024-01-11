#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

#include <boost/signals2.hpp>

namespace monitor
{

class IBaseInterface
{
public:
    boost::signals2::signal<void ()> OnCreateWdtPipe; ///< сигнал создания канала wdt
    static void send_request(const pid_t pid); ///< послать запрос
    static bool m_isTerminate;
protected:
    IBaseInterface();
    virtual ~IBaseInterface();

    typedef std::filesystem::path t_path;
    typedef std::vector<std::string> t_args;
    typedef std::map<pid_t, std::chrono::seconds> t_tasks;
    struct t_prog
    {
        pid_t pid;
        t_path path;
        t_args args;
        bool watched;
    };
    typedef std::vector<t_prog> t_progs;

    bool InitPipe();

    virtual pid_t RunProgram(const t_path& path) const; ///< запустить программу в фоновом режиме
    virtual pid_t RunProgram(const t_path& path, const t_args& args) const; ///< запустить программу в фоновом режиме

    bool PreparePrograms(); ///< подготовить список программ для запуска и наблюдения

    bool TerminateProgram(const pid_t pid) const; ///< завершить выполнение наблюдаемой программы
    pid_t FindTerminatedTask() const; ///< найти внепланово завершившуюся задачу
    bool GetRequestTask(pid_t& pid) const; ///< прочитать новый запрос от задачи
    bool WaitExitAllPrograms() const; ///< дождаться завершения всех программ

    bool ToDaemon() const; ///< демонизировать процесс мониторинга
    void Destroy(); ///< уничтожить созданные в процессе работы объекты

    t_progs& Progs(); ///< получить список наблюдаемых программ
    const t_progs& Progs() const; ///< получить список наблюдаемых программ
private:
    static int m_wdtPipe[2];
    bool m_init = false;
    std::vector<t_prog> m_progs;
};

} // namespace monitor

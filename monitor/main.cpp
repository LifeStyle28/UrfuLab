#include "monitor.h"
#include "base_interface.h"

#include <signal.h>

using namespace monitor;
using namespace boost_logger;
using namespace std::literals;
namespace json = boost::json;
namespace logging = boost::log;

typedef Monitor<IBaseInterface> t_monitor;

static const int SIG_WDT_REG = SIGRTMIN + 1;
static const int SIG_WDT_UNREG = SIGRTMIN + 2;

static void wdthandler(int signo, siginfo_t *info, [[maybe_unused]] void *ptr)
{
    pid_t pid = (signo == SIG_WDT_UNREG) ? -info->si_pid : info->si_pid;
    t_monitor::send_request(pid);
}

static void init_wdt()
{
    struct sigaction wdtact;
    wdtact.sa_sigaction = wdthandler;
    wdtact.sa_flags = SA_SIGINFO;
    sigemptyset(&wdtact.sa_mask);
    sigaddset(&wdtact.sa_mask, SIG_WDT_REG);
    sigaddset(&wdtact.sa_mask, SIG_WDT_UNREG);
    sigaction(SIG_WDT_REG, &wdtact, NULL);
    sigaction(SIG_WDT_UNREG, &wdtact, NULL);
}

int main()
{
    InitLogging();
    // создаем объект мониторинга
    t_monitor monitor;
    // назначаем функцию, которая будет вызвана после создания pipe для wdt
    monitor.OnCreateWdtPipe.connect(init_wdt);

    try
    {
        json::value customData{{}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, customData)
                                << "Monitor has started"sv;
        // @TODO - произвести инициализацию и запустить мониторинг
        if (!monitor.Init())
        {
            boost::json::value custom_data{ {} };
            BOOST_LOG_TRIVIAL(error)
                << boost::log::add_value(boost_logger::additional_data, custom_data)
                << "failed to init monitor!";
            return EXIT_FAILURE;
        }
        if (!monitor.Exec())
        {
            boost::json::value custom_data{};
            BOOST_LOG_TRIVIAL(error) << boost::log::add_value(boost_logger::additional_data, custom_data)
                << "Error in Exec!";
            return EXIT_FAILURE;
        }
    }
    }
    catch (const std::exception& e)
    {
        json::value customData{{"exception"s, e.what()}, {"code"s, EXIT_FAILURE}};
        BOOST_LOG_TRIVIAL(fatal) << logging::add_value(additional_data, customData)
                                << "monitor exited"sv;
        return EXIT_FAILURE;
    }

    json::value customData{{"code"s, 0}};
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, customData)
                            << "monitor exited"sv;

    return EXIT_SUCCESS;
}

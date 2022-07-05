#include <iostream>
#include <fstream>

#include <chrono>

/* Algorithms */
#include <array>
#include <vector>
#include <algorithm>

/* Threads manipulation */
#include <future>
#include <thread>
#include <mutex>

#include <iomanip>

#include <cstdarg>
#include <cstring>

static std::mutex s_release_mutex;

static void release(std::vector<std::ofstream> *output_files,
    std::int64_t wait_for, const char *message)
{
    /* Sleeping... :) */
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_for));

    /* Lock the mutex (Protect data leaks and race conditions) */
    std::lock_guard<std::mutex> lock(s_release_mutex);
    std::vector<std::ofstream>::iterator i;

    for (i = output_files->begin(); i != output_files->end(); i++)
    {
        std::ofstream &outfile = *i;
        outfile.write(message, strlen(message));
    }
    /* Delete the message */
    delete[] message;
}

template<typename A, std::size_t B>
class medusa_log
{
public:
    medusa_log(const std::array<A, B> &lognames)
    {
        for (auto logfile : lognames)
            m_outputs.emplace_back(logfile);
    }
    ~medusa_log()
    {
        std::vector<std::ofstream>::iterator i;
        std::vector<std::future<void>>::iterator fi;

        for (fi = m_futures.begin(); fi != m_futures.end(); fi++)
        {
            std::future<void> &future_obj = *fi;
            future_obj.wait();
        }
        
        for (i = m_outputs.begin(); i != m_outputs.end(); i++)
        {
            std::ofstream &outfile = *i;
            outfile.close();
        }
    }

    void success(const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(SUCCESS, 0, format, list);
        va_end(list);
    }

    void success(std::int64_t wait_for, const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(SUCCESS, wait_for, format, list);
        va_end(list);
    }

    void error(const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(ERROR, 0, format, list);
        va_end(list);
    }

    void error(std::int64_t wait_for, const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(ERROR, wait_for, format, list);
        va_end(list);
    }

    void warning(const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(WARNING, 0, format, list);
        va_end(list);
    }

    void warning(std::int64_t wait_for, const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(WARNING, wait_for, format, list);
        va_end(list);
    }

    void info(const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(INFO, 0, format, list);
        va_end(list);
    }

    void info(std::int64_t wait_for, const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        produce(INFO, wait_for, format, list);
        va_end(list);
    }
    

private:
    enum message_type {
        SUCCESS, ERROR, WARNING, INFO
    };

    void produce(message_type type, std::int64_t wait_for, 
        const char *format, va_list va_format)
    {
        char modified_format[MESSAGE_MAX_SIZE];
        char date_buffer[32];

        const char *str_type = nullptr;

        switch (type)
        {
        case message_type::SUCCESS: str_type = "Success"; break;
        case message_type::ERROR:   str_type = "Error";   break;
        case message_type::WARNING: str_type = "Warning"; break;
        case message_type::INFO:    str_type = "Info";    break;
        }

        auto current_time = std::chrono::system_clock::now();
        const std::time_t message_time = 
            std::chrono::system_clock::to_time_t(current_time + std::chrono::milliseconds(wait_for));
        // https://en.cppreference.com/w/cpp/io/manip/put_time
        std::strftime(date_buffer, sizeof date_buffer, "%T", std::localtime(&message_time));

        std::snprintf(modified_format, sizeof modified_format, "[%s] %s: %s\n", date_buffer, str_type, format);
        
        /* "produce" will eliminate this buffer */
        char *message_buffer = new char[MESSAGE_MAX_SIZE];
        std::vsnprintf(message_buffer, MESSAGE_MAX_SIZE, modified_format, va_format);

        /* Creating a fork of the application and produce the log message */
        m_futures.push_back(std::async(std::launch::async, release, &m_outputs, wait_for, message_buffer));
    }

private:
    std::vector<std::ofstream> m_outputs;
    static constexpr size_t MESSAGE_MAX_SIZE = 256;
    std::vector<std::future<void>> m_futures;
};


std::int32_t main()
{
    const std::array<const char*, 4> log_filenames = {
        "logfile01.log",
        "logfile02.log",
        "logfile03.log",
        "logfile.log"
    };

    medusa_log<const char*, 4> log(log_filenames);
    log.success(2000, "Everything is OK");
    log.error("This is will be present before above log");
    log.warning("There's something strange");

    log.info("Waiting for all thread be finished");
}

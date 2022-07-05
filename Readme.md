# medusalog

A full featured multi-thread and multi-use log system in C

## Features

- Future logs: Wait some seconds for log
- Multi-thread: Every log operation happens in another thread
- Debug-level: Backtrace of where the log happend
- Configurable: You can add/remove (colors, date, program name and other)
- Can write into multiple files
- Can specify full path names

~~~c
/* Available output log message types */
typedef enum 
{
    SUCCESS, INFO, WARNING, ERROR, DEBUG
} medusa_log_type_t;
~~~

~~~c
/* A Simple usage example: */
int main()
{
    const char *logfiles[] = {
        "log/logfile01.log",
        "log/logfile02.log",
        "log/logfile03.log",
        "log/logfile04.log",
        NULL
    };

    medusaattr_t main_attr = {
        /* Display to standard out (my terminal) */
        .usestdout = true,
        
        /* Display the program name */
        .program = "Medusa Example",
        .printprogram = true,

        .printdebug = true,

        .printdate = true,

        .printtype = true,

        /* Display colors :) */
        .colored = true,

        .maxfmt = 150,

        .maxmsg = 200
    };

    medusalog_t *main_log = medusa_new(&main_attr, logfiles, 4);
    
    medusa_log(INFO, main_log, "Main function is located at %p", main);

    medusa_log_await(100, SUCCESS, main_log, "Everything is ok"); 

    medusa_log_await(20, INFO, main_log, "Hmmm this will be printed before the success message"); 

    medusa_log(WARNING, main_log, "I will print a error message, but don't worry, isn't a real error :)");

    medusa_log(ERROR, main_log, "Error message");

    medusa_log_await(120, DEBUG, main_log, "Final message, the log system will be destroyed\n");

    medusa_destroy(main_log);

}
~~~

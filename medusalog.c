#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <execinfo.h>

#include <time.h>

#include <stdarg.h>

#include <stdlib.h>

#include <pthread.h>

#include <string.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct
{
    const char *program;

    bool usestdout,
        printprogram,
        printdebug,
        printdate,
        printtype,
        colored;

    size_t maxfmt;

    size_t maxmsg;

} medusaattr_t;

typedef struct
{    
    const FILE **outfiles;

    pthread_mutex_t release_mutex, mutex;

    medusaattr_t attr;

    size_t wait;

    /* Reusable buffer */
    char *newfmt;

} medusalog_t;

typedef enum 
{
    SUCCESS, INFO, WARNING, ERROR, DEBUG
} medusa_log_type_t;

medusalog_t* medusa_new(medusaattr_t *user_attr, const char **logfilenames, size_t logcount)
{
    medusalog_t *medusa = (medusalog_t*)calloc(sizeof(medusalog_t), 1);
    assert(medusa);

    medusa->outfiles = (const FILE **)calloc(sizeof(const FILE*), logcount + 1);
    assert(medusa->outfiles);

    medusaattr_t *attr = &medusa->attr;

    if (user_attr != NULL)
    {
        memcpy(attr, user_attr, sizeof(medusaattr_t));
    } else
    {
        attr->program = "Medusa LogSystem";
        attr->maxfmt = 150;
        attr->maxmsg = 200;
    }

    medusa->newfmt = malloc(attr->maxfmt);

    const FILE **out = medusa->outfiles;

    char pathbuffer[40];

    while (*logfilenames != NULL)
    {
        strncpy(pathbuffer, *logfilenames, sizeof(pathbuffer));
        char *bak;

        char *pathtree;

        /* strtok_r writes and corrut the address, we need to copy the file path to other location */
        pathtree = strtok_r(pathbuffer, "/", &bak);

        for (; pathtree ;)
        {
            char *save = strtok_r(NULL, "/", &bak);
            if (save == NULL)
                /* We're arive at end of the string */
                break;

            struct stat statbuf;

            if (stat(pathtree, &statbuf) != 0)
                if (mkdir(pathtree, S_IRWXU | S_IRGRP | S_IXGRP | S_IWOTH) != 0)
                    exit(fprintf(stderr, "Can't create %s, because: %s\n", save, strerror(errno)));
            
            pathtree = save;
        }

        /* Create and open all specified files */
        FILE *ptr = fopen(*logfilenames++, "a");
        assert(ptr);
        *out++ = ptr;
    }

    /* Initialize the mutex resource */
    pthread_mutex_init(&medusa->release_mutex, NULL);
    pthread_mutex_init(&medusa->mutex, NULL);

    return medusa;
}

static void medusa_finish(medusalog_t *medusa)
{
    const FILE **out = medusa->outfiles;

    for (; *out != NULL; )
        fclose((FILE*)*out++);
    
}

/* When this function returns, every message has been delivery */
static void medusa_wait(medusalog_t *medusa)
{
    bool quit = false;

    while (quit != true)
    {
        pthread_mutex_lock(&medusa->mutex);
        if (medusa->wait == 0)
            quit = true;
        pthread_mutex_unlock(&medusa->mutex);
    }
}

bool medusa_destroy(medusalog_t *medusa)
{
    assert(medusa);
    //free((char*)medusa->name);

    medusa_wait(medusa);

    medusa_finish(medusa);

    pthread_mutex_lock(&medusa->mutex);
    /* Put the lock in a invalid state to continue */
    pthread_mutex_lock(&medusa->release_mutex);
    /* Never unlocks the mutex */

    free(medusa->outfiles);

    free(medusa->newfmt);

    pthread_mutex_destroy(&medusa->release_mutex);
    pthread_mutex_destroy(&medusa->mutex);
    
    memset(medusa, 0xff, sizeof(medusalog_t));

    free(medusa);

    return true;
}

struct medusa_thread
{
    medusalog_t *medusa;

    char *message;

    size_t sleep_for;

    pthread_t logthread;

};

void* medusa_thread_produce(void *thread_data)
{
    struct medusa_thread *medusa_data = (struct medusa_thread*)thread_data;

    medusalog_t *medusa = medusa_data->medusa;
    medusaattr_t *attr = &medusa->attr;

    struct timespec req = {.tv_sec = 0, .tv_nsec = medusa_data->sleep_for * 1e+6};

    /* Going sleep, I need to do this right now either ;) */
    nanosleep(&req, NULL);

    pthread_mutex_lock(&medusa->mutex);
    pthread_mutex_lock(&medusa->release_mutex);

    const FILE **out = medusa->outfiles;

    char *usermessage = medusa_data->message;

    if (attr->usestdout)
        fprintf(stdout, "%s", usermessage);

    for (; *out != NULL; )
        fwrite(usermessage, strlen(usermessage), 1, (FILE*)*out++);
    
    medusa->wait--;

    free(medusa_data->message);

    free(medusa_data);

    pthread_mutex_unlock(&medusa->release_mutex);
    pthread_mutex_unlock(&medusa->mutex);

    return NULL;
}

int medusa_do(size_t milliseconds, medusa_log_type_t type, medusalog_t *medusa, 
    char *fmt, va_list va_format)
{
    assert(medusa);

    pthread_mutex_lock(&medusa->mutex);

    char *str_type = NULL;

    char str_typebuffer[30];

    switch (type)
    {
    case SUCCESS:   str_type = "Success";   break;
    case INFO:      str_type = "Info";      break;
    case WARNING:   str_type = "Warning";   break;
    case ERROR:     str_type = "Error";     break;
    case DEBUG:     str_type = "Debug";     break;
    default:        str_type = "USER";      break;
    }

    medusaattr_t *attr = &medusa->attr;

    snprintf(str_typebuffer, sizeof(str_typebuffer), "%s%s%s",
        attr->colored ? (
        type == SUCCESS ? "\033[0;32m" :
        type == INFO ? "\033[0;34m" :
        type == WARNING ? "\033[0;33m" :
        type == ERROR ? "\033[0;31m" : 
        type == DEBUG ? "\033[0;35m" : "") : "", 
        str_type, attr->colored ? "\033[0m" : ""
    );

    assert(str_type);

    //char newfmt[attr->maxfmt];

    const char *program = attr->program;

    char date_str[32];

    /* Getting the current date */
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);

    rawtime += milliseconds;

    timeinfo = localtime(&rawtime);

    strftime(date_str, sizeof(date_str), "%T", timeinfo);

    void *stack_buffer[4];
    char **stack_strings;

    int symbol_count = backtrace(stack_buffer, sizeof(stack_buffer)/sizeof(void*));

    stack_strings = backtrace_symbols(stack_buffer, symbol_count);

    char auxbuffer[60];

    const size_t maxlen = attr->maxfmt;

    snprintf(auxbuffer, sizeof(auxbuffer), "%s ", attr->program);
    snprintf(medusa->newfmt, maxlen, "%s?",
        attr->printprogram == true ?  auxbuffer : ""
    );

    snprintf(auxbuffer, sizeof(auxbuffer), "\'%s\' - ", strstr(stack_strings[3], "("));
    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s?",
        attr->printdebug == true ? auxbuffer : ""
    );

    snprintf(auxbuffer, sizeof(auxbuffer), "[%s] ", date_str);
    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s?",
        attr->printdate == true ? auxbuffer  : ""
    );

    snprintf(auxbuffer, sizeof(auxbuffer), "(%s): ", str_typebuffer);
    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s?",
        attr->printtype == true ? auxbuffer : ""
    );

    snprintf(strstr(medusa->newfmt, "?"), maxlen, "%s\n", fmt);

    /*
    snprintf(newfmt, sizeof(newfmt), "%s \'%s\' - [%s] (%s): %s\n", 
        attr->printprogram ? program : "",
        attr->printdebug ? strstr(stack_strings[3], "(") : "",
        attr->printdate ? date_str : "",
        attr->printtype ? str_type : "", 
    fmt);
    */

    free(stack_strings);

    struct medusa_thread *medusa_data = malloc(sizeof(struct medusa_thread));
    
    assert(medusa_data);

    medusa_data->medusa = medusa;

    medusa_data->sleep_for = milliseconds;

    medusa_data->message = (char*)malloc(attr->maxmsg);

    assert(medusa_data->message);

    int ret = vsnprintf(medusa_data->message, attr->maxmsg, medusa->newfmt, va_format);

    medusa->wait++;

    pthread_t thread;

    medusa_data->logthread = pthread_self();

    pthread_mutex_unlock(&medusa->mutex);

    pthread_create(&thread, NULL, medusa_thread_produce, (void*)medusa_data);

    return ret;
}

int medusa_log_await(size_t milliseconds, medusa_log_type_t type, medusalog_t *medusa, char *fmt, ...)
{
    va_list va_format;
    va_start(va_format, fmt);

    int ret = medusa_do(milliseconds, type, medusa, fmt, va_format);

    va_end(va_format);

    return ret;
}

int medusa_log(medusa_log_type_t type, medusalog_t *medusa, char *fmt, ...)
{
    va_list va_format;
    va_start(va_format, fmt);

    int ret = medusa_do(0, type, medusa, fmt, va_format);

    va_end(va_format);

    return ret;
}

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


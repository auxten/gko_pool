/**
 * log.cpp
 *
 *  Created on: 2011-7-13
 *      Author: auxten
 **/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "gingko.h"
#include "log.h"

extern s_gingko_global_t gko;
pthread_mutex_t g_logcut_lock = PTHREAD_MUTEX_INITIALIZER;
/// TLS, for print proformance time in log
static pthread_key_t g_proformace_timer_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

/**
 * @brief loglevel
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static const char * LOG_DIC[] =
    { "FATAL", "WARN ", "NOTIC", "TRACE", "DEBUG", };

/**
 * @brief generate the time string according to the TIME_FORMAT
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
char * gettimestr(char * time, const char * format, struct timeval * tv)
{
    struct tm ltime;
    time_t curtime;
    gettimeofday(tv, NULL);
    curtime = tv->tv_sec;
    ///Format time
    strftime(time, 25, format, localtime_r(&curtime, &ltime));
    return time;
}

static void make_key()
{
    pthread_key_create(&g_proformace_timer_key, NULL);
}

static struct timeval * get_timer(void)
{
    void *p = NULL;

    pthread_once(&key_once, make_key);
    p = pthread_getspecific(g_proformace_timer_key);
    if (p == NULL)
    {
        p = calloc(1, sizeof(struct timeval));
        pthread_setspecific(g_proformace_timer_key, p);
    }

    return (struct timeval *) p;
}

/**
 * @brief log handler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_log_flf(const u_char log_level, const char *file, const int line, const char *func, const char *fmt, ...)
{
    if (gko.opt.to_debug || log_level < DEBUG )
    {
        int errnum = errno;
        va_list args;
        va_start(args, fmt);
        char logstr[256];
        char oldlogpath[MAX_PATH_LEN];
        static FILE * lastfp = NULL;
        static GKO_INT64 counter = 1;
        struct timeval last_timeval;
        struct timeval time_diff;
        struct timeval * time_p = get_timer();
        long usec_diff;

        memcpy(&last_timeval, time_p, sizeof(last_timeval));

        snprintf(logstr, sizeof(logstr), "%s: [%u]", LOG_DIC[log_level], gko_gettid());
        gettimestr(logstr + strlen(logstr), TIME_FORMAT, time_p);
        timersub(time_p, &last_timeval, &time_diff);
        usec_diff = time_diff.tv_sec * 1000000 + time_diff.tv_usec;
        if (usec_diff < 0)
            usec_diff = 0;

        snprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr), "[%s:%d @%s][%ldus]\t", file, line, func, usec_diff);
        vsnprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr), fmt, args);
        if (log_level < NOTICE)
        {
            snprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr),
                    "; ");
            strerror_r(errnum, logstr + strlen(logstr),
                    sizeof(logstr) - strlen(logstr));
        }

        pthread_mutex_lock(&g_logcut_lock);
        if (gko.opt.logpath[0]  == '\0')
        {
            gko.log_fp = stdout;
        }
        else
        {
            counter ++;
            if (counter % MAX_LOG_REOPEN_LINE == 0)
            {
                if (lastfp)
                {
                    fclose(lastfp);
                }
                lastfp = gko.log_fp;
                if (counter % MAX_LOG_LINE == 0)
                {
                    strncpy(oldlogpath, gko.opt.logpath, MAX_PATH_LEN);
                    gettimestr(oldlogpath + strlen(oldlogpath), OLD_LOG_TIME, time_p);
                    rename(gko.opt.logpath, oldlogpath);
                }
                gko.log_fp = fopen(gko.opt.logpath, "a+");
            }
            if(UNLIKELY(! gko.log_fp))
            {
                gko.log_fp = fopen(gko.opt.logpath, "a+");
                if(! gko.log_fp)
                {
                    perror("Cann't open log file");
                    exit(1);
                }
            }
        }
        fprintf(gko.log_fp, "%s\n", logstr);
        fflush(gko.log_fp);
        pthread_mutex_unlock(&g_logcut_lock);

        va_end(args);
    }
    return;

}

int lock_log(void)
{
    return pthread_mutex_lock(&g_logcut_lock);
}

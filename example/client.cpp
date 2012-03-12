/*
 * client.cpp
 *
 *  Created on: Feb 27, 2012
 *      Author: auxten
 */

#include "gko.h"

/// gingko global stuff
s_gingko_global_t gko;
const int T_NUM = 10;

const s_host_t server =
{
    "127.0.0.1",
    2120 };

void * send_test(void *)
{
    char msg[MSG_LEN] = "TEST";
    for (int i = 0; i < 10000; i++)
    {
        if (sendcmd2host(&server, msg, 2, 2) < 0)
        {
            gko_log(FATAL, "sending quit message failed");
        }
    }
//    gko_log(DEBUG, "%s", msg);
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    pthread_attr_t g_attr;
    pthread_t vnode_pthread[T_NUM];
    void *status;
    strncpy(gko.opt.logpath, SERVER_LOG, sizeof(gko.opt.logpath));

    if (pthread_attr_init(&g_attr) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_attr_setdetachstate(&g_attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }

    for (int i = 0; i < T_NUM; i++)
    {
        if (pthread_create(&vnode_pthread[i], &g_attr, send_test, NULL))
        {
            gko_log(FATAL, "download thread %d create error", i);
            return -1;
        }
    }
    for (int i = 0; i < T_NUM; i++)
    {
        if (pthread_join(vnode_pthread[i], &status))
        {
            gko_log(FATAL, "download thread %d join error", i);
            return -1;
        }
        if (status != (void *) 0)
        {
            gko_log(FATAL, "thread %d joined with error num %lld", i,
                    (long long) status);
            return -1;
        }
    }
    return 0;
}


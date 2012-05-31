/*
 * client.cpp
 *
 *  Created on: May 18, 2012
 *      Author: auxten
 */
#include "gko.h"
/// gingko global stuff
s_gingko_global_t gko;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int cnt = 0;

void report_result(void * c, const char * msg)
{
    GKOLOG(NOTICE, "%s", msg);
    pthread_mutex_lock(&lock);
    cnt++;
    pthread_mutex_unlock(&lock);
}

int main(int argc, char** argv)
{
    char cmd[] = "test cmd cmd cmdxxx";
    gko.opt.to_debug = 1;
    gko.ready_to_serv = 1;
    gko.sig_flag = 0;
    gko.opt.worker_thread = 2;
    gko.opt.connlimit = SERV_POOL_SIZE;
//    gko.opt.to_debug = 1;
    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(-1);
    gingko->setOption(&gko.opt);
//    gingko->setProcessHandler(conn_send_data);
    gingko->setReportHandler(report_result);
    gingko->gko_run();

    int i = 100000;

    while (i--)
        gingko->make_active_connect("localhost", 2120, 1, 1, strlen(cmd), cmd);
    while (cnt != i)
    {
        sleep(1);
    }
    return 0;
}



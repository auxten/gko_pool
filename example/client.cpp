/*
 * server.cpp
 *
 *  Created on: May 18, 2012
 *      Author: auxten
 */
#include "gko.h"
/// gingko global stuff
s_gingko_global_t gko;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int cnt = 50;
int counter = 0;
void report_result(void * c, const char * msg)
{
    GKOLOG(NOTICE, "%s", msg);
    pthread_mutex_lock(&lock);
    if (++counter == cnt)
    {
        printf("finished\n");
        exit(0);
    }
    pthread_mutex_unlock(&lock);
}

int main(int argc, char** argv)
{
    char cmd[512];

    gko.opt.to_debug = 0;
    gko.ready_to_serv = 1;
    gko.sig_flag = 0;
    gko.opt.worker_thread = 8;
    gko.opt.connlimit = SERV_POOL_SIZE;
//    gko.opt.to_debug = 1;
    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(-1);
    gingko->setOption(&gko.opt);
//    gingko->setProcessHandler(conn_send_data);
    gingko->setReportHandler(report_result);
    gingko->gko_run();

    int i = cnt;
    while (i--)
    {
        sprintf(cmd, "GET /Poll.php?project_id=5168&id=49 HTTP/1.1\r\n"
                "Host: hi.video.sina.com.cn\r\n"
                "Accept: */*\r\n"
                "Client-IP: %d.%d.%d.1\r\n\r\n", time(NULL) % 255,i % 255, i % 13);
        gingko->make_active_connect("hi.video.sina.com.cn", 80, 1, 1, strlen(cmd), cmd, 0, 10);
    }
    sleep(10);
    return 0;
}



/*
 * client.cpp
 *
 *  Created on: May 18, 2012
 *      Author: auxten
 */
#include "gko.h"
/// gingko global stuff
s_gingko_global_t gko;


void report_result(void * c, const char * msg)
{
    GKOLOG(NOTICE, "%s", msg);
}

int main(int argc, char** argv)
{
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
        gingko->make_active_connect("localhost", 2120, 1, 1, "test cmd cmd cmdxxx");
    sleep(3);
    return 0;
}



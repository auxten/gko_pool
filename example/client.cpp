/*
 * server.cpp
 *
 *  Created on: May 18, 2012
 *      Author: auxten
 */
#include "gko.h"
/// gingko global stuff
s_gingko_global_t gko;


void report_result(void * c, const char * msg)
{
    if (strlen(msg) != 0)
        GKOLOG(NOTICE, "%s", msg);
}


int dispatch_cmd(const char * host, const int port, const long task_id, const long sub_task_id, const char * cmd)
{
    return gko_pool::getInstance()->make_active_connect(host, port, task_id, sub_task_id, cmd);
}

int main(int argc, char** argv)
{
    gko.opt.to_debug = 1;
    gko.ready_to_serv = 1;
    gko.sig_flag = 0;
    gko.opt.port = SERV_PORT;
    gko.opt.worker_thread = 2;
    gko.opt.connlimit = SERV_POOL_SIZE;
    gko.opt.bind_ip = htons(INADDR_ANY);
//    gko.opt.to_debug = 1;
    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(2120);
    gingko->setOption(&gko.opt);
//    gingko->setProcessHandler(conn_send_data);
    gingko->setReportHandler(report_result);
    gingko->gko_run();

    dispatch_cmd("localhost", 2120, 1, 1, "test cmd cmd cmdxxx");
    GKOLOG(DEBUG, "Debug mode start, i will print tons of log :p!");

}



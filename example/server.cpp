/*
 * server.cpp
 *
 *  Created on: May 18, 2012
 *      Author: auxten
 */
#include "gko.h"
/// gingko global stuff
s_gingko_global_t gko;


void * conn_send_data(void * arg)
{
    int process_ret;
    struct conn_client *client = (struct conn_client *) arg;
    char * p = ((char *) client->read_buffer) + CMD_PREFIX_BYTE;
    GKOLOG(DEBUG, "conn_send_data %s", p);

    client->need_write = snprintf(client->write_buffer,
            client->wbuf_size,
            "{\"errno\":\"%d\",\"message\":\"%s\"}",
            0,
            "Hello World");

    return NULL;
}

int dispatch_cmd(const char * host, const int port, const long task_id, const long sub_task_id, const char * cmd)
{
    return gko_pool::getInstance()->make_active_connect(host, port, task_id, sub_task_id, strlen(cmd), cmd);
}

int main(int argc, char** argv)
{
    gko.opt.to_debug = 1;
    gko.ready_to_serv = 1;
    gko.sig_flag = 0;
    gko.opt.worker_thread = 2;
    gko.opt.connlimit = SERV_POOL_SIZE;
    gko.opt.bind_ip = htons(INADDR_ANY);
//    gko.opt.to_debug = 1;
    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(2120);
    gingko->setOption(&gko.opt);
    gingko->setProcessHandler(conn_send_data);
//    gingko->setReportHandler(report_result);


    GKOLOG(DEBUG, "Debug mode start, i will print tons of log :p!");

    return gingko->gko_run();
}


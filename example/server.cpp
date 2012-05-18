/*
 * serv_unittest.cpp
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */

#ifndef GINGKO_SERV
#define GINGKO_SERV
#endif /** GINGKO_SERV **/

#include "gingko.h"
#include "async_pool.h"
#include "hash/xor_hash.h"
#include "log.h"
#include "socket.h"

/// gingko global stuff
s_gingko_global_t gko;


GKO_STATIC_FUNC void * scmd_s(void *p, int)
{
    GKOLOG(DEBUG, "SCMD");
    conn_client * c = (conn_client *) p;

    char * arg_array[3];

    GKOLOG(TRACE, "%x", c);
    if (sep_arg(c->read_buffer + CMD_PREFIX_BYTE, arg_array, 3) != 3)
    {
        GKOLOG(WARNING, "Wrong SCMD cmd: %s", c->read_buffer);
        return (void *) -1;
    }

    GKOLOG(TRACE, "%x", c);

    /// todo write MySQL

    /// add fd to pool
    gko_pool::getInstance()->make_active_connect(arg_array[1], AGENT_PORT, 1, 1, arg_array[2]);
    c->need_write = snprintf(c->write_buffer, c->wbuf_size, "SCMD OK");
    return (void *) 0;
}

void * conn_send_data(void * arg)
{
    struct conn_client *client = (struct conn_client *) arg;
    char * p = ((char *) client->read_buffer) + CMD_PREFIX_BYTE;
    GKOLOG(DEBUG, "conn_send_data %s", p);
    scmd_s(arg, 0);
    return NULL;
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

    GKOLOG(DEBUG, "Debug mode start, i will print tons of log :p!");

    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(2120);
    gingko->setOption(&gko.opt);
    gingko->setProcessHandler(&conn_send_data);

    return gingko->gko_run();
}

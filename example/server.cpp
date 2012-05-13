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

/**
 * @brief server func
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * test_s(void *, int);
GKO_STATIC_FUNC void * scmd_s(void *, int);
GKO_STATIC_FUNC void * g_none_s(void *, int);

/**
 * @brief ************** FUNC DICT **************
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static char g_cmd_list[][CMD_LEN] =
    {
        { "TEST" },
        { "SCMD" },
        { "NONE" }
    };
/**
 * @brief server func list
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static func_t g_func_list_s[] =
    {
        test_s,
        scmd_s,
        g_none_s
    };

GKO_STATIC_FUNC void * test_s(void *p, int)
{
    gko_log(NOTICE, "test");
    conn_client * c = (conn_client *) p;
    c->need_write = snprintf(c->write_buffer, c->wbuf_size, "server test OK");
    return (void *) 0;
}

GKO_STATIC_FUNC void * scmd_s(void *p, int)
{
    gko_log(DEBUG, "SCMD");
    conn_client * c = (conn_client *) p;

    char * arg_array[3];
    s_host_t h;

    if (sep_arg(c->read_buffer, arg_array, 3) != 3)
    {
        gko_log(WARNING, "Wrong SCMD cmd: %s", c->read_buffer);
        return (void *) -1;
    }

    strncpy(h.addr, arg_array[1], sizeof(h.addr) - 1);
    h.addr[sizeof(h.addr) - 1] = '\0';
    h.port = AGENT_PORT;

    /// todo write MySQL

    /// add fd to pool
    gko_pool::getInstance()->make_active_connect(&h, arg_array[2]);
    c->need_write = snprintf(c->write_buffer, c->wbuf_size, "SCMD OK");

    return (void *) 0;
}

GKO_STATIC_FUNC void * g_none_s(void *p, int)
{
    gko_log(NOTICE, "none");
    return (void *) 0;
}


/**
 * @brief server unittest main
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-15
 **/
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

    gko_log(DEBUG, "Debug mode start, i will print tons of log :p!");

    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(2120);
    gingko->setOption(&gko.opt);
    gingko->setFuncTable(g_cmd_list, g_func_list_s, 2);

    return gingko->gko_run();
}

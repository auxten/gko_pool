/*
 * async_clnt.cpp
 *
 *  Created on: May 4, 2012
 *      Author: auxten
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>

#include "config.h"
#include "gingko.h"
#include "log.h"
#include "socket.h"
#ifdef __APPLE__
#include <poll.h>
#else
#include <sys/poll.h>
#endif /** __APPLE__ **/

#include "async_pool.h"
#include "socket.h"

/**
 * @brief non-blocking version connect
 *
 * @see
 * @note
 * @author auxten <auxtenwpc@gmail.com>
 * @date Apr 22, 2012
 **/
int gko_pool::nb_connect(struct conn_client* conn)
{
    int sock = -1;
    struct sockaddr_in channel;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (FAIL_CHECK(sock < 0))
    {
        GKOLOG(FATAL, "get socket error");
        goto NB_CONNECT_END;
    }

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = conn->client_addr;
    channel.sin_port = htons(conn->client_port);

    /** set the connect non-blocking then blocking for add timeout on connect **/
    if (FAIL_CHECK(setnonblock(sock) < 0))
    {
        GKOLOG(FATAL, "set socket non-blocking error");
        goto NB_CONNECT_END;
    }

//    GKOLOG(DEBUG, "before connect");
    char ip[16];
    /** connect and send the msg **/
    if (FAIL_CHECK(connect(sock, (struct sockaddr *) &channel, sizeof(channel)) &&
            errno != EINPROGRESS))
    {
        GKOLOG(WARNING, "connect error %s:%d", addr_itoa(conn->client_addr, ip), conn->client_port);
    }
//    GKOLOG(DEBUG, "after connect");

    NB_CONNECT_END:

    conn->client_fd = sock;
    ///
//    ret = sock;
//    if (ret < 0 && sock >= 0)
//    {
//        close_socket(sock);
//    }
    return sock;
}

int gko_pool::make_active_connect(const char * host, const int port, const long task_id,
        const long sub_task_id, int len, const char * cmd, const u_int8_t flag)
{
    struct conn_client * conn;

    conn = add_new_conn_client(FD_BEFORE_CONNECT);
    if (!conn)
    {
        ///close socket and further receives will be disallowed
        GKOLOG(FATAL, "Server limited: I cannot serve more clients");
        return SERVER_INTERNAL_ERROR;
    }

    int worker_id;
    worker_id = thread_list_find_next();
    if (worker_id < 0)
    {
        GKOLOG(WARNING, "can't find available thread");
        return FAIL;
    }
    else
    {
        conn->worker_id = worker_id;
    }

//    GKOLOG(DEBUG, "conn_buffer_init");
    conn_buffer_init(conn);

    strncpy(conn->client_hostname, host, sizeof(conn->client_hostname) - 1);
    conn->client_hostname[sizeof(conn->client_hostname) - 1] = '\0';
    conn->client_port = port;

    conn->type = active_conn;
    conn->task_id = task_id;
    conn->sub_task_id = sub_task_id;

    conn->need_write = len;
    memcpy(conn->write_buffer, cmd, len);

    thread_worker_dispatch(conn->id, conn->worker_id);
    return SUCC;
}



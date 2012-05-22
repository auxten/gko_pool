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
int gko_pool::nb_connect(const s_host_t * h, struct conn_client* conn)
{
    int sock = -1;
    int ret = -1;
    struct sockaddr_in channel;
    in_addr_t host;
    int addr_len;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (FAIL_CHECK(sock < 0))
    {
        GKOLOG(FATAL, "get socket error");
        goto NB_CONNECT_END;
    }

    GKOLOG(DEBUG, "before getaddr_my");
    addr_len = getaddr_my(h->addr, &host);
    if (FAIL_CHECK(!addr_len))
    {
        GKOLOG(WARNING, "gethostbyname %s error", h->addr);
        goto NB_CONNECT_END;
    }
    GKOLOG(DEBUG, "after getaddr_my");

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, &host, addr_len);
    channel.sin_port = htons(h->port);

    /** set the connect non-blocking then blocking for add timeout on connect **/
    if (FAIL_CHECK(setnonblock(sock) < 0))
    {
        GKOLOG(FATAL, "set socket non-blocking error");
        goto NB_CONNECT_END;
    }

    GKOLOG(DEBUG, "before connect");
    char ip[16];
    /** connect and send the msg **/
    if (FAIL_CHECK(connect(sock, (struct sockaddr *) &channel, sizeof(channel)) &&
            errno != EINPROGRESS))
    {
        GKOLOG(WARNING, "connect error %s:%d", addr_itoa(host, ip), h->port);
    }
    GKOLOG(DEBUG, "after connect");

    NB_CONNECT_END:

    conn->client_addr = host;
    conn->client_port = h->port;
    conn->client_fd = sock;
    ///
//    ret = sock;
//    if (ret < 0 && sock >= 0)
//    {
//        close_socket(sock);
//    }
    return sock;
}

int gko_pool::connect_hosts(const std::vector<s_host_t> & host_vec,
        std::vector<struct conn_client> * conn_vec)
{
    int conn_ok = 0;
    int conn_err = 0;

    if (host_vec.size() <= 0 || conn_vec == NULL)
    {
        return -1;
    }

    conn_vec->clear();

    /// perform non-blocking connect
    for (std::vector<s_host_t>::const_iterator it = host_vec.begin();
            it != host_vec.end();
            it++)
    {
        int nb_conn_sock;
        struct conn_client clnt;
        char ip[16];

        /// connect and set in_addr_t to clnt->client_addr
        nb_conn_sock = nb_connect(&(*it), &clnt);
        if (nb_conn_sock < 0)
        {
//            char ip[18] = {'\0'};
            clnt.client_fd = -1;
            clnt.state = conn_connect_fail;
            conn_err++;
            GKOLOG(DEBUG, "connect fail for %s:%d failed",
                    addr_itoa(clnt.client_addr, ip), clnt.client_port);
        }
        else
        {
            clnt.client_fd = nb_conn_sock;
            clnt.state = conn_connecting;
            conn_ok++;
        }
        conn_vec->push_back(clnt);
    }
    GKOLOG(DEBUG, "first stage connect ok:%d, err:%d", conn_ok, conn_err);

    return 0;
}

int gko_pool::disconnect_hosts(std::vector<struct conn_client> & conn_vec)
{
    for (std::vector<struct conn_client>::iterator it = conn_vec.begin();
            it != conn_vec.end();
            it++)
    {
        if (it->state != conn_closed)
        {
            char ip[16];
            if (close(it->client_fd) != 0)
                GKOLOG(DEBUG, "close fd failed for %s:%d",
                        addr_itoa(it->client_addr, ip), it->client_port);
            it->state = conn_closed;
        }
    }

    return 0;
}

//int gko_pool::fill_request(const char * request, const int req_len, std::vector<struct conn_client> * conn_vec)
//{
//    for (std::vector<struct conn_client>::iterator it = conn_vec->begin();
//            it != conn_vec->end();
//            it++)
//    {
//        if (it->wbuf_size < req_len)
//            strncpy(it->write_buffer, request, req_len);
//    }
//    return 0;
//}

int gko_pool::make_active_connect(const char * host, const int port, const long task_id, const long sub_task_id, int len, const char * cmd)
{
    struct conn_client * conn;
    s_host_t h;

    strncpy(h.addr, host, sizeof(h.addr) - 1);
    h.addr[sizeof(h.addr) - 1] = '\0';
    h.port = port;

    conn = add_new_conn_client(FD_BEFORE_CONNECT);
    if (!conn)
    {
        ///close socket and further receives will be disallowed
        GKOLOG(FATAL, "Server limited: I cannot serve more clients");
        return SERVER_INTERNAL_ERROR;
    }

    GKOLOG(DEBUG, "conn_buffer_init");
    conn_buffer_init(conn);

    conn->need_write = len;
    memcpy(conn->write_buffer, cmd, len);
    conn->type = active_conn;
    conn->task_id = task_id;
    conn->sub_task_id = sub_task_id;

    GKOLOG(DEBUG, "before nb_connect");
    /// non-blocking connect
    int connect_ret = nb_connect(&h, conn);
    if (connect_ret < 0)
    {
        conn->err_no = DISPATCH_SEND_ERROR;
//        if (reportHandler)
//            reportHandler(conn, "");
        GKOLOG(FATAL, "nb_connect ret is %d", connect_ret);
        return DISPATCH_SEND_ERROR;
    }
    GKOLOG(DEBUG, "after nb_connect");


    thread_worker_dispatch(conn->id);
    return SUCC;
}



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

    addr_len = getaddr_my(h->addr, &host);
    if (FAIL_CHECK(!addr_len))
    {
        gko_log(WARNING, "gethostbyname %s error", h->addr);
        ret = -1;
        goto NB_CONNECT_END;
    }
    conn->client_addr = host;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (FAIL_CHECK(sock < 0))
    {
        gko_log(WARNING, "get socket error");
        ret = -1;
        goto NB_CONNECT_END;
    }

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, &host, addr_len);
    channel.sin_port = htons(h->port);

    /** set the connect non-blocking then blocking for add timeout on connect **/
    if (FAIL_CHECK(setnonblock(sock) < 0))
    {
        gko_log(WARNING, "set socket non-blocking error");
        ret = -1;
        goto NB_CONNECT_END;
    }

    /** connect and send the msg **/
    if (FAIL_CHECK(connect(sock, (struct sockaddr *) &channel, sizeof(channel)) &&
            errno != EINPROGRESS))
    {
        gko_log(WARNING, "connect error");
        ret = HOST_DOWN_FAIL;
        goto NB_CONNECT_END;
    }

    ret = sock;

    NB_CONNECT_END:
    ///
    if (ret < 0 && sock >= 0)
    {
        close_socket(sock);
    }
    return ret;
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

        /// connect and set in_addr_t to clnt->client_addr
        nb_conn_sock = nb_connect(&(*it), &clnt);
        if (nb_conn_sock < 0)
        {
            char ip[18] = {'\0'};
            clnt.client_fd = -1;
            clnt.state = conn_connect_fail;
            conn_err++;
            gko_log(DEBUG, "connect fail for %s:%d failed",
                    inet_neta(clnt.client_addr, ip, 17), clnt.client_port);
        }
        else
        {
            clnt.client_fd = nb_conn_sock;
            clnt.state = conn_connecting;
            conn_ok++;
        }
        conn_vec->push_back(clnt);
    }
    gko_log(DEBUG, "first stage connect ok:%d, err:%d", conn_ok, conn_err);

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
            char ip[18] = {'\0'};
            if (close(it->client_fd) != 0)
                gko_log(DEBUG, "close fd failed for %s:%d",
                        inet_neta(it->client_addr, ip, 17), it->client_port);
            it->state = conn_closed;
        }
    }
    return 0;
}

int gko_pool::fill_request(const char * request, const int req_len, std::vector<struct conn_client> * conn_vec)
{
    for (std::vector<struct conn_client>::iterator it = conn_vec->begin();
            it != conn_vec->end();
            it++)
    {

    }
    return 0;
}

/*
 * async_clnt.cpp
 *
 *  Created on: May 4, 2012
 *      Author: auxten
 */

#include "async_pool.h"
#include "socket.h"

int connect_hosts(const std::vector<s_host_t> & host_vec,
        std::vector<struct conn_client> * conn_vec)
{
    struct conn_client conn;
    memset(&conn, 0, sizeof(conn));
    int conn_ok = 0;
    int conn_err = 0;

    if (host_vec.size() <= 0 || conn_vec == NULL)
    {
        return -1;
    }
    /// perform non-blocking connect
    for (std::vector<s_host_t>::iterator it = host_vec.begin();
            it != host_vec.end();
            it++)
    {
        int nb_conn_sock;
        struct conn_client clnt;

        clnt.client_addr = it->
        nb_conn_sock = nb_connect(&(*it));
        if (nb_conn_sock < 0)
        {
            clnt.client_fd = -1;
            clnt.state = conn_connect_fail;
            conn_err++;
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

int disconnect_hosts(std::vector<struct conn_client> & conn_vec)
{
    for(std::vector<struct conn_client>::iterator it = conn_vec.begin();
            it != conn_vec.end();
            it++)
    {
        close(it->client_fd)
    }

    return 0;
}

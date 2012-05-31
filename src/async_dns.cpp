/*
 * async_dns.cpp
 *
 *  Created on: May 22, 2012
 *      Author: auxten
 */

#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "event.h"
#include "ares.h"
#include "gko.h"

void gko_pool::dns_callback(void* arg, int status, int timeouts, struct hostent* host)
{
    conn_client *c = (conn_client *) arg;

    if (status == ARES_SUCCESS)
    {
        u_int32_t addr = *(in_addr_t *) host->h_addr;
        c->client_addr = addr;
        conn_set_state(c, conn_connecting);
        GKOLOG(DEBUG, "DNS OK %s ==> %u.%u.%u.%u",
                host->h_name,
                (addr) % 256, (addr >> 8) % 256, (addr >> 16) % 256, (addr >> 24) % 256);
    }
    else
    {
        c->err_no = DNS_RESOLVE_FAIL;
        conn_set_state(c, conn_closing);
        GKOLOG(WARNING, "lookup failed: %s", ares_strerror(status));
    }
    event_del(&c->ev_dns);

    /// call my lovly state_machine~~~ mua~
    state_machine(c);
}

void gko_pool::dns_ev_callback(int fd, short ev, void *arg)
{
    ares_channel ch = ((thread_worker *) arg)->dns_channel;
    if (ev & EV_READ)
        ares_process_fd(ch, fd, ARES_SOCKET_BAD);
    else if (ev & EV_WRITE)
        ares_process_fd(ch, ARES_SOCKET_BAD, fd);
}

void gko_pool::nb_gethostbyname(conn_client *c)
{
    ares_socket_t *read_fds;
    ares_socket_t *write_fds;

//    gko_pool * Pool = gko_pool::getInstance();
    thread_worker * worker = *(g_worker_list + c->worker_id);

    ares_gethostbyname(worker->dns_channel, c->client_hostname, AF_INET, dns_callback, (void *) c);
    ares_fds_array(worker->dns_channel, &read_fds, &write_fds);

    for (int i = 0; *(read_fds + i) != ARES_SOCKET_BAD; i++)
    {
        event_set(&c->ev_dns, *(read_fds + i), EV_READ | EV_PERSIST, dns_ev_callback,
                (void *) (worker));
    }
    for (int i = 0; *(write_fds + i) != ARES_SOCKET_BAD; i++)
    {
        event_set(&c->ev_dns, *(write_fds + i), EV_WRITE | EV_PERSIST, dns_ev_callback,
                (void *) (worker));
    }

    free(read_fds);
    free(write_fds);
    event_base_set(worker->ev_base, &c->ev_dns);
    event_add(&c->ev_dns, 0);
}

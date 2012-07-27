/*
 * async_dns.cpp
 *
 *  Created on: May 22, 2012
 *      Author: auxten
 */

#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <vector>

#include "event.h"
#include "ares.h"
#include "gko.h"

int gko_pool::del_dns_event(conn_client *c)
{
    for (std::vector<struct event *>::iterator it = c->ev_dns_vec.begin();
            it != c->ev_dns_vec.end();
            ++it)
    {
        event_del(*it);
        free(*it);
    }
    c->ev_dns_vec.clear();
    return 0;
}

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
    del_dns_event(c);

    /// call my lovly state_machine~~~ mua~
    state_machine(c);
}

void gko_pool::dns_ev_callback(int fd, short ev, void *arg)
{
    gko_pool * Pool = gko_pool::getInstance();
    conn_client *c = (conn_client *) arg;
    thread_worker * w = *(Pool->g_worker_list + c->worker_id);

    ares_channel ch = w->dns_channel;
    if (ev & EV_READ)
        ares_process_fd(ch, fd, ARES_SOCKET_BAD);
    if (ev & EV_WRITE)
        ares_process_fd(ch, ARES_SOCKET_BAD, fd);
    if (ev & EV_TIMEOUT)
    {
        c->err_no = DNS_RESOLVE_FAIL;
        conn_set_state(c, conn_closing);
        GKOLOG(FATAL, "dns timeout");
        del_dns_event(c);
        state_machine(c);
    }
}

void gko_pool::nb_gethostbyname(conn_client *c)
{
    ares_socket_t *read_fds;
    ares_socket_t *write_fds;
    struct timeval timeout = {5, 0};

//    gko_pool * Pool = gko_pool::getInstance();
    thread_worker * worker = *(g_worker_list + c->worker_id);

    ares_gethostbyname(worker->dns_channel, c->client_hostname, AF_INET, dns_callback, (void *) c);
    ares_fds_array(worker->dns_channel, &read_fds, &write_fds);

    if ((*read_fds == *write_fds) && (*write_fds == ARES_SOCKET_BAD))
    {
        GKOLOG(DEBUG, "error no fd");
        del_dns_event(c);
        return;
    }

    for (int i = 0; *(read_fds + i) != ARES_SOCKET_BAD; i++)
    {
        struct event * ev_tmp = (struct event *)malloc(sizeof(struct event));
        event_set(ev_tmp, *(read_fds + i), EV_READ | EV_PERSIST, dns_ev_callback,
                (void *) (c));
        event_base_set(worker->ev_base, ev_tmp);
        event_add(ev_tmp, &timeout);

        c->ev_dns_vec.push_back(ev_tmp);
    }
    for (int i = 0; *(write_fds + i) != ARES_SOCKET_BAD; i++)
    {
        struct event * ev_tmp = (struct event *)malloc(sizeof(struct event));
        event_set(ev_tmp, *(write_fds + i), EV_WRITE | EV_PERSIST, dns_ev_callback,
                (void *) (c));
        event_base_set(worker->ev_base, ev_tmp);
        event_add(ev_tmp, &timeout);

        c->ev_dns_vec.push_back(ev_tmp);
    }

    free(read_fds);
    free(write_fds);
}

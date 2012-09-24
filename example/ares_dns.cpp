/*
 * async_dns.cpp
 *
 *  Created on: May 22, 2012
 *      Author: auxten
 */

#include <time.h>
#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "event.h"
#include "ares.h"
#include "gko.h"

struct event_base* ev_base;
struct event ev_dns;

void dns_callback(void* arg, int status, int timeouts, struct hostent* host)
{
    if (status == ARES_SUCCESS)
    {
        u_int32_t addr = *(in_addr_t *) host->h_addr;
        printf("%u.%u.%u.%u\n", (addr) % 256, (addr >> 8) % 256, (addr >> 16) % 256, (addr >> 24) % 256);
    }
    else
    {
        std::cout << "lookup failed: " << status << '\n';
    }
    event_del(&ev_dns);
//    event_base_loopexit(ev_base, &t);
}

void dns_ev_callback(int fd, short ev, void *arg)
{
    if (ev & EV_READ)
        ares_process_fd((ares_channel) arg, fd, ARES_SOCKET_BAD);
    else if (ev & EV_WRITE)
        ares_process_fd((ares_channel) arg, ARES_SOCKET_BAD, fd);
}

void main_loop(ares_channel channel)
{
    int nfds, count;
    fd_set readers, writers;
    ev_base = (struct event_base*) event_init();

    ares_socket_t *read_fds;
    ares_socket_t *write_fds;
//    FD_ZERO(&readers);
//    FD_ZERO(&writers);
////        ares_
//    nfds = ares_fds(channel, &readers, &writers);
//    for (int i = 0; i < readers.fd_count; i++)
//    {
//        if (FD_ISSET(readers.fd_array[i], readers))
//            event_set(&ev_dns, 1, EV_READ, ev_callback,
//                    (void *) (&channel));
//    }
//    for (int i = 0; i < writers.fd_count; i++)
//    {
//        if (FD_ISSET(writers.fd_array[i], writers))
//            event_set(&ev_dns, 1, EV_WRITE, ev_callback,
//                    (void *) (&channel));
//    }

////////////////////
//    struct server_state *server;
//
//    /* Are there any active queries? */
//    int active_queries = !ares__is_list_empty(&(channel->all_queries));
//
//    for (int i = 0; i < channel->nservers; i++)
//    {
//        server = &channel->servers[i];
//        /* We only need to register interest in UDP sockets if we have
//         * outstanding queries.
//         */
//        if (active_queries && server->udp_socket != ARES_SOCKET_BAD)
//        {
////            FD_SET(server->udp_socket, read_fds);
//            event_set(&ev_dns, server->udp_socket, EV_READ, ev_callback,
//                    (void *) (&channel));
//        }
//        /* We always register for TCP events, because we want to know
//         * when the other side closes the connection, so we don't waste
//         * time trying to use a broken connection.
//         */
//        if (server->tcp_socket != ARES_SOCKET_BAD)
//        {
////           FD_SET(server->tcp_socket, read_fds);
//            event_set(&ev_dns, server->tcp_socket, EV_READ, ev_callback,
//                    (void *) (&channel));
//            if (server->qhead)
////             FD_SET(server->tcp_socket, write_fds);
//                event_set(&ev_dns, server->tcp_socket, EV_WRITE, ev_callback,
//                        (void *) (&channel));
//        }
//    }
////////////////////
    ares_fds_array(channel, &read_fds, &write_fds);

    for (int i = 0; *(read_fds + i) != ARES_SOCKET_BAD; i++)
    {
//        printf( "read %d\n", *(read_fds + i));
        event_set(&ev_dns, *(read_fds + i), EV_READ | EV_PERSIST, dns_ev_callback,
                (void *) (channel));
    }
    for (int i = 0; *(write_fds + i) != ARES_SOCKET_BAD; i++)
    {
//        printf("write %d\n", *(write_fds + i));
        event_set(&ev_dns, *(write_fds + i), EV_WRITE | EV_PERSIST, dns_ev_callback,
                (void *) (channel));
    }

    free(read_fds);
    free(write_fds);
    event_base_set(ev_base, &ev_dns);
    event_add(&ev_dns, 0);
    event_base_loop(ev_base, 0);

//    timeval tv, *tvp;
//    while (1)
//    {
//        FD_ZERO(&readers);
//        FD_ZERO(&writers);
////        ares_
//        nfds = ares_fds(channel, &readers, &writers);
//        if (nfds == 0)
//            break;
//        tvp = ares_timeout(channel, NULL, &tv);
//        count = select(nfds, &readers, &writers, NULL, tvp);
//        ares_process(channel, &readers, &writers);
//    }

}
int main(int argc, char **argv)
{
    struct in_addr ip;
    int res;
    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " ip.address\n";
        return 1;
    }
//    inet_aton(argv[1], &ip);
    ares_library_init(ARES_LIB_INIT_ALL);
    ares_channel channel;
    if ((res = ares_init(&channel)) != ARES_SUCCESS)
    {
        std::cout << "ares feiled: " << res << '\n';
        return 1;
    }

    ares_gethostbyname(channel, argv[1], AF_INET, dns_callback, NULL);
    main_loop(channel);
    return 0;
}


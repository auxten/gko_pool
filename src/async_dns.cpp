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
#include "dict.h"
#include "gko.h"

static const time_t        DNS_EXPIRE_TIME =           -1;

typedef struct DNSCacheVal
{
    time_t expire_time;
    in_addr_t addr;
} DNSCacheVal;

/* And a case insensitive str hash function (based on dictGenCaseHashFunction hash) */
unsigned int dictGenStrCaseHashFunction(const void *p)
{
    unsigned char *buf = (unsigned char *) p;
    unsigned int hash = (unsigned int) DICT_HASH_FUNCTION_SEED;

    while (*buf != '\0')
        hash = ((hash << 5) + hash) + (tolower(*buf++)); /* hash * 33 + c */
    return hash;
}

int dictStrCaseKeyCompare(void *privdata, const void *key1, const void *key2)
{
    DICT_NOTUSED(privdata);
    return strcasecmp((const char *) key1, (const char *) key2) == 0;
}

void * strKeyDup(void *privdata, const void *key)
{
    DICT_NOTUSED(privdata);
    return (void *) strdup((const char *) key);
}

void strKeyDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);
    free(key);
}

void strValDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);
    free(key);
}

dictType DNSDictType =
    {
        dictGenStrCaseHashFunction, /* hash function */
        strKeyDup, /* key dup */
        NULL, /* val dup */
        dictStrCaseKeyCompare, /* key compare */
        strKeyDestructor, /* key destructor */
        strValDestructor /* val destructor */
    };

dict * gko_pool::init_dns_cache(void)
{
    return dictCreate(&DNSDictType, NULL);
}

/**
 *  try if hostname hit the cache, so we save a query
 * @param conn_client *
 * @return if hit cache return 0, elsewise -1
 */
int gko_pool::try_dns_cache(conn_client *c)
{
    int ret;
    if (DNS_EXPIRE_TIME <= 0)
    {
        ret = -1;
        return ret;
    }

    DNSCacheVal *val;
    time_t now = time(NULL);

    pthread_mutex_lock(&DNS_cache_lock);
    val = (DNSCacheVal *)dictFetchValue(DNSDict, c->client_hostname);
    if (val != NULL)
    {
        /// hit !
        if (val->expire_time < now)
        {
            /// TTL expire
            dictDelete(DNSDict, c->client_hostname);
            ret = -1;
        }
        else
        {
            /// within TTL
            c->client_addr = val->addr;
            ret = 0;
        }
    }
    else
    {
        /// not hit
        ret = -1;
    }
    pthread_mutex_unlock(&DNS_cache_lock);
    return ret;
}

void gko_pool::update_dns_cache(conn_client *c, in_addr_t addr)
{
    if (DNS_EXPIRE_TIME <= 0)
    {
        return;
    }
    pthread_mutex_lock(&DNS_cache_lock);
    DNSCacheVal * val = (DNSCacheVal *)malloc(sizeof (DNSCacheVal));
    val->addr = addr;
    val->expire_time = time(NULL) + DNS_EXPIRE_TIME; ///
    dictReplace(DNSDict, c->client_hostname, val);
    pthread_mutex_unlock(&DNS_cache_lock);
}

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
        gko_pool::getInstance()->update_dns_cache(c, addr);
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

    if (c->state == conn_closing)
    {
        GKOLOG(FATAL, "closing socket have DNS event");
        del_dns_event(c);
        state_machine(c);
        return;
    }
    ares_channel ch = w->dns_channel;
    if (ev & EV_READ)
        ares_process_fd(ch, fd, ARES_SOCKET_BAD);
    if (ev & EV_WRITE)
        ares_process_fd(ch, ARES_SOCKET_BAD, fd);
    if (ev & EV_TIMEOUT)
    {
        ares_process_fd(ch, fd, fd);
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
//    struct timeval timeout = {5, 0};

//    gko_pool * Pool = gko_pool::getInstance();
    thread_worker * worker = *(g_worker_list + c->worker_id);

    ares_gethostbyname(worker->dns_channel, c->client_hostname, AF_INET, dns_callback, (void *) c);
    ares_fds_array(worker->dns_channel, &read_fds, &write_fds);

    if ((*read_fds == *write_fds) && (*write_fds == ARES_SOCKET_BAD))
    {
//        GKOLOG(DEBUG, "error no fd");
        del_dns_event(c);
        free(read_fds);
        free(write_fds);
        return;
    }

    for (int i = 0; *(read_fds + i) != ARES_SOCKET_BAD; i++)
    {
        struct event * ev_tmp = (struct event *) malloc(sizeof(struct event));
        event_set(ev_tmp, *(read_fds + i), EV_READ | EV_PERSIST, dns_ev_callback,
                (void *) (c));
        event_base_set(worker->ev_base, ev_tmp);
        event_add(ev_tmp, NULL);

        c->ev_dns_vec.push_back(ev_tmp);
    }
    for (int i = 0; *(write_fds + i) != ARES_SOCKET_BAD; i++)
    {
        struct event * ev_tmp = (struct event *) malloc(sizeof(struct event));
        event_set(ev_tmp, *(write_fds + i), EV_WRITE | EV_PERSIST, dns_ev_callback,
                (void *) (c));
        event_base_set(worker->ev_base, ev_tmp);
        event_add(ev_tmp, NULL);

        c->ev_dns_vec.push_back(ev_tmp);
    }

    free(read_fds);
    free(write_fds);
}

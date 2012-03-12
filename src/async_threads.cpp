/**
 *  async_threads.cpp
 *  gingko
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 *
 **/

#include "gingko.h"
#include "async_pool.h"
#include "log.h"

/**
 * @brief create new thread worker
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::thread_worker_new(int id)
{
    int ret;

    struct thread_worker *worker = new struct thread_worker;
    if (!worker)
    {
        gko_log(FATAL, "new thread_worker failed");
        return -1;
    }

    int fds[2];
    if (pipe(fds) != 0)
    {
        gko_log(FATAL, "pipe error");
        return -1;
    }
    worker->notify_recv_fd = fds[0];
    worker->notify_send_fd = fds[1];

    worker->ev_base = (struct event_base*) event_init();
    if (!worker->ev_base)
    {
        gko_log(FATAL, "Worker event base initialize error");
        return -1;
    }

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setstacksize(&thread_attr, MYSTACKSIZE);
    ///pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&worker->tid, &thread_attr, thread_worker_init,
            (void *) worker);
    if (ret)
    {
        gko_log(FATAL, "Thread create error");
        return -1;
    }
    worker->id = id;
    *(g_worker_list + id) = worker;
    ///gko_log(NOTICE, "thread_worker_new :%d",(*(g_worker_list+id))->notify_send_fd );
    return 0;
}

/**
 * @brief Worker initialize
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void * gko_pool::thread_worker_init(void *arg)
{
    struct thread_worker *worker = (struct thread_worker *) arg;
    event_set(&worker->ev_notify, worker->notify_recv_fd, EV_READ | EV_PERSIST,
            thread_worker_process, worker);
    event_base_set(worker->ev_base, &worker->ev_notify);
    event_add(&worker->ev_notify, 0);
    event_base_loop(worker->ev_base, 0);

    return NULL;
}

/**
 * @brief Transfer a new event to worker
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_pool::thread_worker_process(int fd, short ev, void *arg)
{
    struct thread_worker *worker = (struct thread_worker *) arg;
    int c_id;
    read(fd, &c_id, sizeof(int));
    struct conn_client *client = gko_pool::getInstance()->conn_client_list_get(
            c_id);

    if (client->client_fd)
    {
        event_set(&client->event, client->client_fd, EV_READ | EV_PERSIST,
                conn_tcp_server_on_data, (void *) client);
        event_base_set(worker->ev_base, &client->event);
        if (-1 == event_add(&client->event, 0))
        {
            gko_log(WARNING, "Cannot handle client's data event");
        }
    }
    else
    {
        gko_log(WARNING, "conn_client_list_get error");
    }
    return;
}

/**
 * @brief find an availiable thread, return thread index; on error
 *          return -1
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::thread_list_find_next()
{
    int i;
    int tmp;

    for (i = 0; i < option->worker_thread; i++)
    {
        tmp = (i + g_curr_thread + 1) % option->worker_thread;
        if (*(g_worker_list + tmp) && (*(g_worker_list + tmp))->tid)
        {
            g_curr_thread = tmp;
            return tmp;
        }
    }
    gko_log(WARNING, "thread pool full");
    return -1;
}

/**
 * @brief init the whole thread pool
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::thread_init()
{
    int i;
    g_worker_list = new struct thread_worker *[option->worker_thread];
    if (!g_worker_list)
    {
        gko_log(FATAL,
                "new new struct thread_worker *[option->worker_thread] failed");
        return -1;
    }
    memset(g_worker_list, 0,
            sizeof(struct thread_worker *) * option->worker_thread);
    for (i = 0; i < option->worker_thread; i++)
    {
        if (thread_worker_new(i) != 0)
        {
            gko_log(FATAL, FLF("thread_worker_new error"));
            return -1;
        }
    }

    return 0;
}

/**
 * @brief parse the request return the proper func handle num
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::parse_req(char *req)
{
    int i;
    if (UNLIKELY(!req))
    {
        return cmd_count - 1;
    }
    for (i = 0; i < cmd_count - 1; i++)
    {
        if (cmd_list_p[i][0] == req[0] && //todo use int
                cmd_list_p[i][1] == req[1] &&
                cmd_list_p[i][2] == req[2] &&
                cmd_list_p[i][3] == req[3])
        {
            break;
        }
    }
    return i;
}

/**
 * @brief Dispatch to worker
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_pool::thread_worker_dispatch(int c_id)
{
    int worker_id;
    int res;
    worker_id = thread_list_find_next();
    if (worker_id < 0)
    {
        gko_log(WARNING, "can't find available thread");
        return;
    }
    res = write((*(g_worker_list + worker_id))->notify_send_fd, &c_id,
            sizeof(int));
    if (res == -1)
    {
        gko_log(WARNING, "Pipe write error");
    }
}

int gko_pool::gko_loopexit(int timeout)
{
    struct timeval timev;

    timev.tv_sec = timeout;
    timev.tv_usec = 0;
    event_base_loopexit(g_ev_base, (timeout ? &timev : NULL));
    return 0;
}

void gko_pool::event_handler(const int fd, const short which, void *arg)
{
    conn_client *c;

    assert(c != NULL);
    c = (conn *) arg;

    state_machine(c);

    /* wait for next event */
    return;
}

bool gko_pool::update_event(conn_client *c, const int new_flags)
{
    assert(c != NULL);

    struct event_base *base = c->event.ev_base;
    if (c->ev_flags == new_flags)
        return true;
    if (event_del(&c->event) == -1)
        return false;
    event_set(&c->event, c->client_fd, new_flags, worker_event_handler,
            (void *) c);
    event_base_set(base, &c->event);
    c->ev_flags = new_flags;
    if (event_add(&c->event, 0) == -1)
        return false;
    return true;
}

/*
 * Sets a connection's current state in the state machine. Any special
 * processing that needs to happen on certain state transitions can
 * happen here.
 */
void gko_pool::conn_set_state(conn_client *c, enum conn_states state)
{
    assert(c != NULL);
    assert(state >= conn_listening && state < conn_max_state);

    if (state != c->state)
    {
        gko_log(DEBUG, "stat#%d going to stat#%d", c->state, state);
        c->state = state;
    }
}

void gko_pool::state_machine(conn_client *c)
{
    bool stop = false;
    int sfd, flags = 1;
    socklen_t addrlen;
    struct sockaddr_storage addr;
    int nreqs = settings.reqs_per_event;
    int res;

    assert(c != NULL);

    while (!stop)
    {

        switch (c->state)
        {
            case conn_listening:
                gko_log(WARNING, "listening state again?!");
                break;

            case conn_waiting:
                if (!update_event(c, EV_READ | EV_PERSIST))
                {
                    if (settings.verbose > 0)
                        fprintf(stderr, "Couldn't update event\n");
                    conn_set_state(c, conn_closing);
                    break;
                }

                conn_set_state(c, conn_read);
                stop = true;
                break;

            case conn_read:
                res = try_read_network(c);

                switch (res)
                {
                    case READ_NO_DATA_RECEIVED:
                        conn_set_state(c, conn_waiting);
                        break;
                    case READ_DATA_RECEIVED:
                        conn_set_state(c, conn_parse_cmd);
                        break;
                    case READ_ERROR:
                        conn_set_state(c, conn_closing);
                        break;
                    case READ_MEMORY_ERROR: /* Failed to allocate more memory */
                        /* State already set by try_read_network */
                        break;
                }
                break;

            case conn_parse_cmd:
                if (try_read_command(c) == 0)
                {
                    /* wee need more data! */
                    conn_set_state(c, conn_waiting);
                }

                break;

            case conn_new_cmd:
                /* Only process nreqs at a time to avoid starving other
                 connections */

                --nreqs;
                if (nreqs >= 0)
                {
                    reset_cmd_handler(c);
                }
                else
                {
                    if (c->rbytes > 0)
                    {
                        /* We have already read in data into the input buffer,
                         so libevent will most likely not signal read events
                         on the socket (unless more data is available. As a
                         hack we should just put in a request to write data,
                         because that should be possible ;-)
                         */
                        if (!update_event(c, EV_WRITE | EV_PERSIST))
                        {
                            if (settings.verbose > 0)
                                fprintf(stderr, "Couldn't update event\n");
                            conn_set_state(c, conn_closing);
                        }
                    }
                    stop = true;
                }
                break;

            case conn_swallow:

            case conn_write:

            case conn_mwrite:

            case conn_closing:
                conn_client_free(c);
                stop = true;
                break;

            case conn_max_state:
                assert(false);
                break;
        }
    }

    return;
}


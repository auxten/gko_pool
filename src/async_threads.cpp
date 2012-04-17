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
    struct conn_client *client =
        gko_pool::getInstance()->conn_client_list_get(c_id);

    /// todo calloc every connection comes?
    client->read_buffer = (char *)calloc(RBUF_SZ, sizeof(char));
    client->rbuf_size = RBUF_SZ;
    client->have_read = 0;
    client->need_read = CMD_PREFIX_BYTE;

    /// todo calloc every connection comes?
    client->__write_buffer = (char *)calloc(WBUF_SZ + CMD_PREFIX_BYTE, sizeof(char));
    client->write_buffer = client->__write_buffer + CMD_PREFIX_BYTE;
    client->wbuf_size = WBUF_SZ;
    client->have_write = 0;
    client->__need_write = CMD_PREFIX_BYTE;
    client->need_write = 0;

    if (client->client_fd)
    {
        event_set(&client->event, client->client_fd, EV_READ | EV_PERSIST,
                worker_event_handler, (void *) client);
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

void gko_pool::worker_event_handler(const int fd, const short which, void *arg)
{
    conn_client *c;

    assert(c != NULL);
    c = (conn_client *) arg;

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

enum aread_result gko_pool::aread(conn_client *c)
{
    enum aread_result gotdata = READ_NO_DATA_RECEIVED;
    int res;
    int num_allocs = 0;
    assert(c != NULL);

    while (1)
    {
        if (c->have_read >= c->need_read)
        {
            if (num_allocs == 4)
            {
                return gotdata;
            }
            ++num_allocs;
            char *new_rbuf = (char *)realloc(c->read_buffer, c->rbuf_size * 2);
            if (!new_rbuf)
            {
                gko_log(FATAL, FLF("Couldn't realloc input buffer"));
                c->have_read = 0; /* ignore what we read */
                gko_log(FATAL, FLF("SERVER_ERROR out of memory reading request"));
                return READ_MEMORY_ERROR;
            }
            c->read_buffer = new_rbuf;
            c->rbuf_size *= 2;
        }

        int avail = c->rbuf_size - c->have_read;
        res = read(c->client_fd, c->read_buffer + c->have_read, avail);
        if (res > 0)
        {
            c->have_read += res;

            if (c->need_read > c->have_read)
            {
                gotdata = READ_NEED_MORE;
            }
            else /// read enough
            {
                if (c->need_read == CMD_PREFIX_BYTE)
                {
                    gotdata = READ_HEADER_RECEIVED;
                }
                else if (c->need_read > CMD_PREFIX_BYTE)
                {
                    gotdata = READ_DATA_RECEIVED;
                }
                else /// c->need_read < CMD_PREFIX_BYTE
                {
                    gko_log(FATAL, "You just need me to read %d bytes??",
                            c->need_read);
                    gotdata = READ_ERROR;
                }
            }

            if (res == avail)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        if (res == 0)
        {
            return READ_ERROR;
        }
        if (res == -1)
        {
            if (ERR_RW_RETRIABLE(errno))
            {
                break;
            }
            return READ_ERROR;
        }
    }
    return gotdata;
}

enum awrite_result gko_pool::awrite(conn_client *c)
{
    enum awrite_result write_result = WRITE_NO_DATA_SENT;
    int res;
    assert(c != NULL);

//    struct iovec iov[2];
//    struct msghdr msg;
//
//    iov[0] .iov_base = (caddr_t)head;
//    iov[0] .iov_len = sizeof(struct header);
//    iov[1] .iov_base = (caddr_t)trans;
//    iov[1] .iov_len = sizeof(struct record);
//
//       /* The message header contains parameters for sendmsg.    */
//    mh.msg_name = NULL;
//    mh.msg_namelen = 0;
//    mh.msg_iov = iov;
//    mh.msg_iovlen = 2;
//    mh.msg_accrights = NULL;            /* irrelevant to AF_INET */
//    mh.msg_accrightslen = 0;            /* irrelevant to AF_INET */

    while (1)
    {
        int to_send = c->__need_write - c->have_write;
        res = send(c->client_fd, c->__write_buffer + c->have_write, to_send, 0);

        if (res > 0)
        {
            c->have_write += res;

            if (c->__need_write > c->have_write)
            {
                write_result = WRITE_SENT_MORE;
                continue;
            }
            else if (c->__need_write == c->have_write) /// write enough
            {
                write_result = WRITE_DATA_SENT;
                break;
            }
        }
        if (res == 0)
        {
            return WRITE_ERROR;
        }
        if (res == -1)
        {
            if (ERR_RW_RETRIABLE(errno))
            {
                break;
            }
            return WRITE_ERROR;
        }
    }
    return write_result;
}

void gko_pool::state_machine(conn_client *c)
{
    bool stop = false;
    int nreqs = 3;
    enum aread_result res;
    enum awrite_result ret;
    unsigned short proto_ver;
    unsigned int msg_len = 0;

    assert(c != NULL);

    while (!stop)
    {

        switch (c->state)
        {
            case conn_listening:
                gko_log(DEBUG, "state: conn_listening");
                gko_log(WARNING, "listening state again?!");
                break;

            case conn_waiting:
                gko_log(DEBUG, "state: conn_waiting");
                if (!update_event(c, EV_READ | EV_PERSIST))
                {
                    gko_log(WARNING, "Couldn't update event");
                    conn_set_state(c, conn_closing);
                    break;
                }

                conn_set_state(c, conn_read);
                stop = true;
                break;

            case conn_read:
                gko_log(DEBUG, "state: conn_read");
                res = aread(c);

                switch (res)
                {
                    case READ_NO_DATA_RECEIVED:
                        conn_set_state(c, conn_waiting);
                        break;
                    case READ_HEADER_RECEIVED:
                        conn_set_state(c, conn_parse_header);
                        break;
                    case READ_DATA_RECEIVED:
                        conn_set_state(c, conn_parse_cmd);
                        break;
                    case READ_NEED_MORE:
                        break;
                    case READ_ERROR:
                        conn_set_state(c, conn_closing);
                        break;
                    case READ_MEMORY_ERROR: /* Failed to allocate more memory */
                        c->need_write = snprintf(c->write_buffer, c->wbuf_size,
                                "SERVER_ERROR out of memory reading request");
                        conn_set_state(c, conn_write);
                        break;
                }
                break;

            case conn_parse_header:
                gko_log(DEBUG, "state: conn_parse_header");
                parse_cmd_head(c->read_buffer, &proto_ver, &msg_len);
                if (msg_len > 0)
                {
                    c->need_read = msg_len;
                    if (c->have_read < c->need_read)
                    {
                        conn_set_state(c, conn_read);
                    }
                    else if (c->have_read == c->need_read)
                    {
                        conn_set_state(c, conn_parse_cmd);
                    }
                    else /* have read more than expect */
                    {
                        conn_set_state(c, conn_parse_cmd);
                        gko_log(NOTICE, FLF("have read more than expect"));
                    }
                }
                else
                {
                    gko_log(NOTICE, FLF("parse cmd head failed"));
                    c->need_write = snprintf(c->write_buffer, c->wbuf_size,
                            "parse cmd head failed");
                    conn_set_state(c, conn_write);
                }

                break;

            case conn_parse_cmd:     /**< try to parse a command from the input buffer */
                gko_log(DEBUG, "state: conn_parse_cmd");
                if (gko_pool::getInstance()->g_server->on_data_callback)
                {
                    gko_pool::getInstance()->g_server->on_data_callback((void *) c);
                }
                conn_set_state(c, conn_write);
                if (!update_event(c, EV_WRITE | EV_PERSIST))
                {
                    gko_log(FATAL, "Couldn't update event");
                    conn_set_state(c, conn_closing);
                    break;
                }
                if (nreqs-- == 0)
                {
                    stop = true;
                }
                break;

            case conn_nread:         /**< reading in a fixed number of bytes */
                gko_log(DEBUG, "state: conn_nread");
                gko_log(FATAL, "NOT Supported yet :p");
                break;

            case conn_write:
                gko_log(DEBUG, "state: conn_write");
                c->__need_write = c->need_write + CMD_PREFIX_BYTE;
                fill_cmd_head(c->__write_buffer, c->__need_write);

                ret = awrite(c);

                switch (ret)
                {
                    case WRITE_DATA_SENT:
                        conn_set_state(c, conn_closing);
                        break;

                    case WRITE_HEADER_SENT: /// fall through
                    case WRITE_SENT_MORE:
                        break;
                    case WRITE_NO_DATA_SENT:
                        gko_log(FATAL, FLF("write no data sent"));
                        conn_set_state(c, conn_closing);
                        break;

                    case WRITE_ERROR:
                        gko_log(FATAL, FLF("write to socket error"));
                        conn_set_state(c, conn_closing);
                        break;
                }
                break;

            case conn_mwrite:
                gko_log(DEBUG, "state: conn_mwrite");
                gko_log(FATAL, "NOT Supported yet :p");
                break;

            case conn_closing:
                gko_log(DEBUG, "state: conn_closing");
                gko_pool::getInstance()->conn_client_free(c);
                stop = true;
                break;

            case conn_max_state:
                gko_log(DEBUG, "state: conn_max_state");
                assert(false);
                break;

        }
    }

    return;
}


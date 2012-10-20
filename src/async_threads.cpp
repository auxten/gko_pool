/**
 *  async_threads.cpp
 *  gingko
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 *
 **/

#include "gingko.h"
#include "socket.h"
#include "async_pool.h"
#include "log.h"


/// put conn into current thread conn_set
void thread_worker::add_conn(int c_id)
{
    this->conn_set.insert(c_id);
}

/// del conn from current thread conn_set
void thread_worker::del_conn(int c_id)
{
    this->conn_set.erase(c_id);
}

int defaultHMTRHandler(void * p, const char * buf, const int len)
{

    return 0;
}


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
    int dns_ret;
    struct ares_options dns_opt;

    thread_worker *worker = new thread_worker;
    if (!worker)
    {
        GKOLOG(FATAL, "new thread_worker failed");
        return -1;
    }

    worker->userData = NULL;
    int fds[2];
    if (pipe(fds) != 0)
    {
        GKOLOG(FATAL, "pipe error");
        return -1;
    }
    worker->notify_recv_fd = fds[0];
    worker->notify_send_fd = fds[1];

    worker->ev_base = (struct event_base*) event_init();
    if (!worker->ev_base)
    {
        GKOLOG(FATAL, "Worker event base initialize error");
        return -1;
    }

    dns_opt.flags = 0;//ARES_FLAG_PRIMARY; /// set while clean other flags
    dns_ret = ares_init_options(&worker->dns_channel, &dns_opt, ARES_OPT_FLAGS);
    if (dns_ret != ARES_SUCCESS)
    {
        GKOLOG(FATAL, "Ares_init failed: %s", ares_strerror(dns_ret));
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
        GKOLOG(FATAL, "Thread create error");
        return -1;
    }
    worker->id = id;
    *(g_worker_list + id) = worker;
    ///GKOLOG(NOTICE, "thread_worker_new :%d",(*(g_worker_list+id))->notify_send_fd );
    return 0;
}

void gko_pool::clean_handler(const int fd, const short which, void *arg)
{
    thread_worker *worker = (thread_worker *) arg;
    struct timeval t = {0, 0};
    time_t current_time;

    evtimer_del(&worker->ev_cleantimeout);
    evtimer_set(&worker->ev_cleantimeout, clean_handler, worker);
    event_base_set(worker->ev_base, &worker->ev_cleantimeout);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    current_time = tv.tv_sec;
//    GKOLOG(DEBUG, "now time is %ld", current_time);

    gko_pool::getInstance()->clean_conn_timeout(worker, current_time);

    t.tv_sec = 5 + tv.tv_usec % 4;
    evtimer_add(&worker->ev_cleantimeout, &t);
}

int gko_pool::clean_conn_timeout(thread_worker *worker, time_t now)
{
    int timeout_cnt = 0;
    gko_pool * Pool = gko_pool::getInstance();
    std::list<int> timeout_client_l;

    for (std::set<int>::const_iterator it = worker->conn_set.begin();
            it != worker->conn_set.end();
            ++it)
    {
        int conn_id = *it;
        conn_client * conn = *(g_client_list + conn_id);
        if (conn->conn_time && now - conn->conn_time > NET_TIMEOUT)
        {
            timeout_cnt++;
            if (conn->type == coming_conn)
            {
                if (conn->state == conn_waiting || conn->state == conn_read)
                {
                    GKOLOG(NOTICE, "read income conn timeout, clean up");
                    timeout_client_l.push_back(conn->id);
                    Pool->conn_client_free(conn);
                }
                else if (conn->state == conn_write)
                {
                    GKOLOG(NOTICE, "write income conn timeout, clean up");
                    timeout_client_l.push_back(conn->id);;
                    Pool->conn_client_free(conn);
                }
            }
            else if (conn->type == active_conn)
            {
                if (conn->state == conn_read)
                {
                    GKOLOG(NOTICE, "read active conn timeout, clean up");
                    conn->err_no = DISPATCH_RECV_TIMEOUT;
                    if (reportHandler)
                        reportHandler(conn, "");
                    timeout_client_l.push_back(conn->id);;
                    Pool->conn_client_free(conn);
                }
                else if (conn->state == conn_write || conn->state == conn_connecting)
                {
                    GKOLOG(NOTICE, "write active conn timeout, clean up");
                    conn->err_no = DISPATCH_SEND_TIMEOUT;
                    if (reportHandler)
                        reportHandler(conn, "");
                    timeout_client_l.push_back(conn->id);;
                    Pool->conn_client_free(conn);
                }
            }
        }
    }

    for (std::list<int>::const_iterator it = timeout_client_l.begin();
            it != timeout_client_l.end();
            ++it)
    {
        worker->del_conn((*(g_client_list + *it))->id);
    }

    return timeout_cnt;
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
    thread_worker *worker = (thread_worker *) arg;
    event_set(&worker->ev_notify, worker->notify_recv_fd, EV_READ | EV_PERSIST,
            thread_worker_process, worker);
    event_base_set(worker->ev_base, &worker->ev_notify);
    event_add(&worker->ev_notify, 0);

    /// set clean_handler first time, then it will set itself
    evtimer_set(&worker->ev_cleantimeout, clean_handler, worker);
    event_base_set(worker->ev_base, &worker->ev_cleantimeout);
    struct timeval t = {5, 0};
    evtimer_add(&worker->ev_cleantimeout, &t);

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
    thread_worker *worker = (thread_worker *) arg;
    int c_id;
    read(fd, &c_id, sizeof(int));
    gko_pool * Pool = gko_pool::getInstance();
    struct conn_client *client =
            Pool->conn_client_list_get(c_id);

    worker->add_conn(c_id);
    client->worker_id = worker->id;

    if (client->type == coming_conn)
    {
        conn_buffer_init(client);
        client->state = conn_waiting;

        if (client->client_fd)
        {
            event_set(&client->event, client->client_fd, EV_READ | EV_PERSIST,
                    worker_event_handler, (void *) client);
            event_base_set(worker->ev_base, &client->event);
            if (-1 == event_add(&client->event, 0))
            {
                GKOLOG(WARNING, "Cannot handle client's data event");
            }
        }
        else
        {
            GKOLOG(WARNING, "conn_client_list_get error");
        }
    }
    else if (client->type == active_conn)
    {
        client->state = conn_resolving;
//        GKOLOG(DEBUG, "conn_resolving");

        Pool->nb_gethostbyname(client);
    }
}

thread_worker * gko_pool::getWorker(const struct conn_client * client)
{
    return *(g_worker_list + client->worker_id);
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
    int tmp = -1;

    pthread_mutex_lock(&thread_list_lock);
    for (i = 0; i < option->worker_thread; i++)
    {
        tmp = (i + g_curr_thread + 1) % option->worker_thread;
        if (*(g_worker_list + tmp) && (*(g_worker_list + tmp))->tid)
        {
            g_curr_thread = tmp;
            break;
        }
    }
    pthread_mutex_unlock(&thread_list_lock);

//    if (i == option->worker_thread)
//    {
//        GKOLOG(WARNING, "thread pool full");
//        tmp = -1;
//    }
    return tmp;
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
    g_worker_list = new thread_worker *[option->worker_thread];
    if (!g_worker_list)
    {
        GKOLOG(FATAL,
                "new thread_worker *[option->worker_thread] failed");
        return -1;
    }
    memset(g_worker_list, 0,
            sizeof(thread_worker *) * option->worker_thread);
    for (i = 0; i < option->worker_thread; i++)
    {
        if (thread_worker_new(i) != 0)
        {
            GKOLOG(FATAL, FLF("thread_worker_new error"));
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Dispatch to the specified worker
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_pool::thread_worker_dispatch(int c_id, int worker_id)
{
    int res;

    if (worker_id < 0)
    {
        GKOLOG(WARNING, "can't find available thread");
        return;
    }
    res = write((*(g_worker_list + worker_id))->notify_send_fd, &c_id,
            sizeof(int));
    if (res == -1)
    {
        GKOLOG(WARNING, "Pipe write error");
    }
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
    worker_id = thread_list_find_next();
    thread_worker_dispatch(c_id, worker_id);
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

    assert(arg != NULL);
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
    assert(state > conn_nouse && state < conn_max_state);

    if (state != c->state)
    {
//        GKOLOG(DEBUG, "stat#%d going to stat#%d", c->state, state);
        c->state = state;
        if (state == conn_closing &&
            c->task_id >= 0 &&
            c->sub_task_id >= 0 &&
            c->err_no != INVILID)
        {
            if (gko_pool::getInstance()->reportHandler)
                gko_pool::getInstance()->reportHandler(c, "");
        }
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
        /// if hava_read == rbuf_size then realloc
        if (c->have_read >= c->rbuf_size) /// c->have_read > c->rbuf_size may not happen
        {
            if (num_allocs++ == 10)
            {
                return READ_MEMORY_ERROR;
            }

            if (c->r_buf_arena_id >= 0)
            {
                char * buf = (char *)malloc(c->rbuf_size * 2);
                if (!buf)
                {
                    GKOLOG(FATAL, FLF("Couldn't alloc input buffer"));
                    c->have_read = 0; /* ignore what we read */
                    return READ_MEMORY_ERROR;
                }
                memcpy(buf, c->read_buffer, c->rbuf_size);

                gko_pool * Pool = gko_pool::getInstance();
                thread_worker * worker = *(Pool->g_worker_list + c->worker_id);
                worker->mem.free_block(c->r_buf_arena_id);
                c->r_buf_arena_id = INVILID_BLOCK;
                c->read_buffer = buf;
                c->rbuf_size *= 2;
                /// after that the following realloc should do nothing :)
            }
            else
            {
                char *new_rbuf = (char *)realloc(c->read_buffer, c->rbuf_size * 2);
                if (!new_rbuf)
                {
                    GKOLOG(FATAL, FLF("Couldn't realloc input buffer"));
                    c->have_read = 0; /* ignore what we read */
                    GKOLOG(FATAL, FLF("SERVER_ERROR out of memory reading request"));
                    return READ_MEMORY_ERROR;
                }
                c->read_buffer = new_rbuf;
                c->rbuf_size *= 2;
            }
        }

        int avail = c->rbuf_size - c->have_read;
        res = read(c->client_fd, c->read_buffer + c->have_read, MIN(c->need_read - c->have_read, (unsigned int)avail));
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
                    GKOLOG(FATAL, "You just need me to read %d bytes??",
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
    int msg_len = 0;
    int connect_ret;
    char ip[16];

    assert(c != NULL);
    gko_pool * Pool = gko_pool::getInstance();
    thread_worker * worker = *(Pool->g_worker_list + c->worker_id);

    while (!stop)
    {

        switch (c->state)
        {
            case conn_listening:
                GKOLOG(DEBUG, "state: conn_listening");
                GKOLOG(WARNING, "listening state again?!");
                break;

//            case conn_resolving:
//                GKOLOG(DEBUG, "state: conn_resolving");
//                stop = true;
//                break;

            case conn_connecting:
//                GKOLOG(DEBUG, "state: conn_connecting");
//                GKOLOG(DEBUG, "before nb_connect");
                /// non-blocking connect
                connect_ret = Pool->nb_connect(c);
                if (connect_ret < 0)
                {
                    GKOLOG(FATAL, "nb_connect ret is %d", connect_ret);
                    c->err_no = DISPATCH_SEND_ERROR;
                    conn_set_state(c, conn_closing);
                    break;
                }

                if (c->client_fd >= 0)
                {
                    event_set(&c->event, c->client_fd, EV_WRITE | EV_PERSIST,
                            worker_event_handler, (void *) c);
                    event_base_set(worker->ev_base, &c->event);
                    if (-1 == event_add(&c->event, 0))
                    {
                        GKOLOG(WARNING, "Cannot handle client's data event");
                    }
                    conn_set_state(c, conn_write);
                    stop = true;
                }
                else
                {
                    GKOLOG(FATAL, "invilid client fd %d", c->client_fd);
                    c->err_no = DISPATCH_SEND_ERROR;
                    conn_set_state(c, conn_closing);
                    break;
                }

//                GKOLOG(DEBUG, "after nb_connect");
                break;

            case conn_waiting:
//                GKOLOG(DEBUG, "state: conn_waiting");
                if (!update_event(c, EV_READ | EV_PERSIST))
                {
                    GKOLOG(WARNING, "Couldn't update event");
                    c->err_no = SERVER_INTERNAL_ERROR;
                    conn_set_state(c, conn_closing);
                    break;
                }

                conn_set_state(c, conn_read);
                stop = true;
                break;

            case conn_read:
//                GKOLOG(DEBUG, "state: conn_read");
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
                        if (c->type == active_conn)
                        {
                            c->err_no = DISPATCH_RECV_ERROR;
                        }
                        conn_set_state(c, conn_closing);
                        break;
                    case READ_MEMORY_ERROR: /* Failed to allocate more memory */
                        GKOLOG(FATAL, "SERVER_ERROR out of memory reading request");
                        c->err_no = SERVER_INTERNAL_ERROR;
                        conn_set_state(c, conn_closing);
                        break;
                }
                break;

            case conn_parse_header:
//                GKOLOG(DEBUG, "state: conn_parse_header");
                parse_cmd_head(c->read_buffer, &proto_ver, &msg_len);
                if (msg_len > 0)
                {
                    c->need_read = msg_len + CMD_PREFIX_BYTE;
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
                        GKOLOG(NOTICE, "have read more than expect %u > %u", c->have_read, c->need_read);
                    }
                }
                else
                {
                    *(c->read_buffer + c->rbuf_size - 1) = '\0';
                    GKOLOG(WARNING, "parse cmd head failed: %s", c->read_buffer);
                    if (c->type == active_conn)
                    {
                        c->err_no = DISPATCH_RECV_ERROR;
                    }
                    else
                    {
                        c->err_no = RECV_ERROR;
                    }
                    conn_set_state(c, conn_closing);
                }

                break;

            case conn_parse_cmd:     /**< try to parse a command from the input buffer */
//                GKOLOG(DEBUG, "state: conn_parse_cmd");

                if (c->type == coming_conn)
                {
                    if (Pool->g_server && Pool->g_server->on_data_callback)
                    {
                        Pool->g_server->on_data_callback((void *) c);
                    }

                    c->conn_time = time(NULL);
                    conn_set_state(c, conn_write);
                    if (!update_event(c, EV_WRITE | EV_PERSIST))
                    {
                        GKOLOG(FATAL, "Couldn't update event");
                        c->err_no = SERVER_INTERNAL_ERROR;
                        conn_set_state(c, conn_closing);
                        break;
                    }
                }
                else if (c->type == active_conn)
                {
                    c->err_no = INVILID;
                    if (Pool->g_server && Pool->g_server->on_data_callback)
                    {
                        Pool->g_server->on_data_callback((void *) c);
                    }
                    conn_set_state(c, conn_closing);
                    break;
                }

                if (nreqs-- == 0)
                {
                    stop = true;
                }
                break;

            case conn_nread:         /**< reading in a fixed number of bytes */
//                GKOLOG(DEBUG, "state: conn_nread");
                GKOLOG(FATAL, "NOT Supported yet :p");
                break;

            case conn_write:
//                GKOLOG(DEBUG, "state: conn_write");
                c->__need_write = c->need_write + CMD_PREFIX_BYTE;
                if (!c->have_write)
                    fill_cmd_head(c->__write_buffer, c->need_write);

                ret = awrite(c);

                switch (ret)
                {
                    case WRITE_DATA_SENT:
                        if (c->type == coming_conn)
                        {
                            conn_set_state(c, conn_state_renew);
                        }
                        else if (c->type == active_conn)
                        {
                            c->conn_time = time(NULL);
                            conn_set_state(c, conn_read);
                        }
                        break;

                    case WRITE_HEADER_SENT: /// fall through
                    case WRITE_SENT_MORE:
                        stop = true;
                        break;
                    case WRITE_NO_DATA_SENT:
                        GKOLOG(FATAL, "write no data sent");
                        c->err_no = SERVER_INTERNAL_ERROR;
                        conn_set_state(c, conn_closing);
                        break;

                    case WRITE_ERROR:
                        GKOLOG(WARNING, "write to socket error, closing");
                        if (c->type == active_conn)
                        {
                            c->err_no = DISPATCH_SEND_ERROR;
                        }
                        conn_set_state(c, conn_closing);
                        break;
                }
                break;

            case conn_state_renew:
//                GKOLOG(DEBUG, "state: conn_state_reset");
                Pool->conn_renew(c);
                conn_set_state(c, conn_waiting);
                break;

            case conn_mwrite:
//                GKOLOG(DEBUG, "state: conn_mwrite");
                GKOLOG(FATAL, "NOT Supported yet :p");
                break;

            case conn_closing:
                GKOLOG(DEBUG, "state: conn_closing %s:%d", addr_itoa(c->client_addr, ip), c->client_port);

                worker->del_conn(c->id);
                Pool->conn_client_free(c);
                stop = true;
                break;

            case conn_max_state:
//                GKOLOG(DEBUG, "state: conn_max_state");
                assert(false);
                break;

            default:
                GKOLOG(DEBUG, "state: %d", c->state);
                assert(false);
                break;


        }
    }

    return;
}


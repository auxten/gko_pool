/**
 *  async_conn.cpp
 *  gingko
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 *
 **/

#include "gingko.h"
#include "async_pool.h"
#include "log.h"
#include "socket.h"

gko_pool * gko_pool::_instance = NULL;
/// init global lock
pthread_mutex_t gko_pool::instance_lock = PTHREAD_MUTEX_INITIALIZER;
/// init global connecting list lock
pthread_mutex_t gko_pool::conn_list_lock = PTHREAD_MUTEX_INITIALIZER;
/// init global thread list lock
pthread_mutex_t gko_pool::thread_list_lock = PTHREAD_MUTEX_INITIALIZER;
/// init gko_pool::gko_serv
s_host_t gko_pool::gko_serv = {"\0", 0};

/**
 * @brief Initialization of client list
 *
 * @see
 * @note
 * @author wangpengcheng01
 * @date 2011-8-1
 **/
int gko_pool::conn_client_list_init()
{
    g_client_list = new conn_client *[option->connlimit];
    memset(g_client_list, 0, option->connlimit * sizeof(struct conn_client *));
    g_total_clients = 0;
    g_total_connect = 0;

    GKOLOG(NOTICE, "Client pool initialized as %d", option->connlimit);

    return option->connlimit;
}

/**
 * @brief Init for gingko_clnt
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::gko_async_server_base_init()
{
    g_server = new conn_server;
    memset(g_server, 0, sizeof(struct conn_server));

    g_server->srv_addr = option->bind_ip;
    if (this->port < 0)
    {
        unsigned int randseed = (unsigned int)getpid();
        int pt = MAX_LIS_PORT - rand_r(&randseed) % 500;
        g_server->srv_port = pt;
    }
    else
    {
        g_server->srv_port = this->port;
    }
    g_server->listen_queue_length = REQ_QUE_LEN;
    g_server->nonblock = 1;
    g_server->tcp_nodelay = 1;
    g_server->tcp_reuse = 1;
    g_server->tcp_send_buffer_size = TCP_BUF_SZ;
    g_server->tcp_recv_buffer_size = TCP_BUF_SZ;
    g_server->send_timeout = SND_TIMEOUT;
    g_server->on_data_callback = pHandler;
    g_server->is_server = this->port < 0 ? 0 : 1;
    /// A new TCP server
    int ret;
    while ((ret = conn_tcp_server(g_server)) < 0)
    {
        if (ret == BIND_FAIL)
        {
            if (g_server->srv_port < MIN_PORT || this->port >= 0)
            {
                GKOLOG(FATAL, "bind port failed, last try is %d", port);
                return -1;
            }
            g_server->srv_port --;
            usleep(BIND_INTERVAL);
        }
        else
        {
            GKOLOG(FATAL, "conn_tcp_server start error");
            return -1;
        }
    }

    set_sig(int_handler);

    ares_library_init(ARES_LIB_INIT_ALL);/// not thread safe

    gko_serv.port = g_server->srv_port;
    return g_server->srv_port;
}

/**
 * @brief Generate a TCP server by given struct
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::conn_tcp_server(struct conn_server *c)
{
    if (g_server->srv_port > MAX_PORT)
    {
        GKOLOG(FATAL, "serv port %d > %d", g_server->srv_port, MAX_PORT);
        return -1;
    }

    /// If port number below 1024, root privilege needed
    if (g_server->srv_port < MIN_PORT)
    {
        /// CHECK ROOT PRIVILEGE
        if (ROOT != geteuid())
        {
            GKOLOG(FATAL, "Port %d number below 1024, root privilege needed",
                    g_server->srv_port);
            return -1;
        }
    }

    /// Create new socket
    g_server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server->listen_fd < 0)
    {
        GKOLOG(WARNING, "Socket creation failed");
        return -1;
    }

    g_server->listen_addr.sin_family = AF_INET;
    g_server->listen_addr.sin_addr.s_addr = g_server->srv_addr;
    g_server->listen_addr.sin_port = htons(g_server->srv_port);

    /// Bind socket
    if (bind(g_server->listen_fd, (struct sockaddr *) &g_server->listen_addr,
            sizeof(g_server->listen_addr)) < 0)
    {
        if (g_server->is_server)
        {
            GKOLOG(FATAL, "Socket bind failed on port");
            return -1;
        }
        else
        {
            return BIND_FAIL;
        }
    }
    GKOLOG(NOTICE, "Upload port bind on port %d", g_server->srv_port);

    /// Listen socket
    if (listen(g_server->listen_fd, g_server->listen_queue_length) < 0)
    {
        GKOLOG(FATAL, "Socket listen failed");
        return -1;
    }

    /// Set socket options
    struct timeval send_timeout;
    send_timeout.tv_sec = g_server->send_timeout;
    send_timeout.tv_usec = 0;

    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &g_server->tcp_reuse,
            sizeof(g_server->tcp_reuse));
    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_SNDTIMEO,
            (char *) &send_timeout, sizeof(struct timeval));
    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_SNDBUF,
            &g_server->tcp_send_buffer_size, sizeof(g_server->tcp_send_buffer_size));
    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_RCVBUF,
            &g_server->tcp_recv_buffer_size, sizeof(g_server->tcp_recv_buffer_size));
    setsockopt(g_server->listen_fd, IPPROTO_TCP, TCP_NODELAY,
            (char *) &g_server->tcp_nodelay, sizeof(g_server->tcp_nodelay));

    /// Set socket non-blocking
    if (g_server->nonblock && setnonblock(g_server->listen_fd) < 0)
    {
        GKOLOG(WARNING, "Socket set non-blocking failed");
        return -1;
    }

    g_server->start_time = time((time_t *) NULL);

    ///GKOLOG(WARNING, "Socket server created on port %d", server->srv_port);
    ///g_ev_base = event_init();
    /// Add data handler
    event_set(&g_server->ev_accept, g_server->listen_fd, EV_READ | EV_PERSIST,
            conn_tcp_server_accept, (void *) c);
    event_base_set(g_ev_base, &g_server->ev_accept);
    event_add(&g_server->ev_accept, NULL);
    ///event_base_loop(g_ev_base, 0);
    return g_server->listen_fd;
}

/**
 * @brief Accept new connection
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_pool::conn_tcp_server_accept(int fd, short ev, void *arg)
{
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct conn_client *client;
    char ip[16];
    ///struct conn_server *server = (struct conn_server *) arg;
    /// Accept new connection
    client_fd = accept(fd, (struct sockaddr *) &client_addr, &client_len);
    if (-1 == client_fd)
    {
        GKOLOG(WARNING, "Accept error");
        return;
    }
    /// Add connection to event queue
    client = gko_pool::getInstance()->add_new_conn_client(client_fd);
    if (!client)
    {
        ///close socket and further receives will be disallowed
        shutdown(client_fd, SHUT_RD);
        close(client_fd);
        GKOLOG(WARNING, "Server limited: I cannot serve more clients");
        return;
    }

    /// set blocking
    ///fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)& ~O_NONBLOCK);

    /// Try to set non-blocking
    if (setnonblock(client_fd) < 0)
    {
        gko_pool::getInstance()->conn_client_free(client);
        GKOLOG(FATAL, "Client socket set non-blocking error");
        return;
    }

    /// Client initialize
    client->client_addr = inet_addr(inet_ntoa(client_addr.sin_addr));
    client->client_port = ntohs(client_addr.sin_port);
    client->type = coming_conn;

    GKOLOG(DEBUG, "coming conn %s:%d",
            addr_itoa(client->client_addr, ip), client->client_port);

    gko_pool::getInstance()->thread_worker_dispatch(client->id);

    return;
}

/**
 * @brief ADD New connection client
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
struct conn_client * gko_pool::add_new_conn_client(int client_fd)
{
    int id;
    struct conn_client *tmp = (struct conn_client *) NULL;
    /// Find a free slot
    id = conn_client_list_find_free();
    GKOLOG(DEBUG, "add_new_conn_client id %d",id);///test
    if (id >= 0)
    {
        tmp = g_client_list[id];
    }
    else
    {
        /// FIXME
        /// Client list pool full, if you want to enlarge it,
        /// modify async_pool.h source please
        GKOLOG(FATAL, "Client list full");
        return tmp;
    }

    if (tmp)
    {
        tmp->id = id;
        tmp->client_fd = client_fd;
        g_client_list[id] = tmp;
        if (client_fd == FD_BEFORE_CONNECT)
        {
            g_total_connect++;
        }
        else
        {
            g_total_clients++;
        }
    }
    return tmp;
}

/**
 * @brief Find a free slot from client pool
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::conn_client_list_find_free()
{
    int i;
    int tmp = -1;

    pthread_mutex_lock(&conn_list_lock);
    for (i = 0; i < option->connlimit; i++)
    {
        tmp = (i + g_curr_conn + 1) % option->connlimit;
        if (!g_client_list[tmp] || 0 == g_client_list[tmp]->conn_time)
        {
            if (!g_client_list[tmp])
            {
                g_client_list[tmp] = new conn_client;
                memset(g_client_list[tmp], 0, sizeof(conn_client));
            }

            conn_client_clear(g_client_list[tmp]);
            g_client_list[tmp]->conn_time = time((time_t *) NULL);

            g_curr_conn = tmp;
            break;
        }
    }
    pthread_mutex_unlock(&conn_list_lock);

    if (i == option->connlimit)
        tmp = -1;
    return tmp;
}

/**
 * @brief Close a client and free all data
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::conn_client_free(struct conn_client *client)
{
    if (!client || !client->client_fd)
    {
        return -1;
    }
    ///close socket and further receives will be disallowed
    shutdown(client->client_fd, SHUT_RD);
    close(client->client_fd);
    conn_client_clear(client);
    g_total_clients--;

    return 0;
}

void gko_pool::conn_buffer_init(conn_client *client)
{
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
}

/**
 * @brief Empty client struct data
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::conn_client_clear(struct conn_client *client)
{
    if (client)
    {
        /// Clear all data
        client->client_fd = 0;
        client->client_addr = 0;
        client->client_port = 0;
        client->err_no = SUCC;

        /// todo free every connection comes?
        if (client->read_buffer)
        {
            free(client->read_buffer);
            client->read_buffer = NULL;
        }
        client->rbuf_size = RBUF_SZ;
        client->have_read = 0;
        client->need_read = CMD_PREFIX_BYTE;

        /// todo free every connection comes?
        if (client->__write_buffer)
        {
            free(client->__write_buffer);
            client->__write_buffer = NULL;
        }
        client->wbuf_size = WBUF_SZ;
        client->have_write = 0;
        client->__need_write = CMD_PREFIX_BYTE;
        client->need_write = 0;

        /// Delete event
        event_del(&client->event);
        /**
         * this is the flag of client usage,
         * we must put it in the last place
         */
        client->conn_time = 0;
        return 0;
    }
    return -1;
}

/**
 * @brief Get client object from pool by given client_id
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
struct conn_client * gko_pool::conn_client_list_get(int id)
{
    return g_client_list[id];
}


gko_pool::gko_pool(const int pt)
    :
        g_curr_thread(0),
        g_curr_conn(0),
        g_server(NULL),
        port(pt),
        pHandler(NULL),
        reportHandler(NULL)
{
    g_ev_base = (struct event_base*)event_init();
    if (!g_ev_base)
    {
        GKOLOG(FATAL, "event init failed");
    }
}

gko_pool::gko_pool()
    :
        g_curr_thread(0),
        g_curr_conn(0),
        g_server(NULL),
        port(-1),
        pHandler(NULL),
        reportHandler(NULL)
{
    g_ev_base = (struct event_base*)event_init();
    if (!g_ev_base)
    {
        GKOLOG(FATAL, "event init failed");
    }
}

int gko_pool::getPort() const
{
    return port;
}

void gko_pool::setPort(int port)
{
    this->port = port;
}

s_option_t *gko_pool::getOption() const
{
    return option;
}

void gko_pool::setOption(s_option_t *option)
{
    this->option = option;
}

void gko_pool::setProcessHandler(ProcessHandler_t process_func)
{
    this->pHandler = process_func;
}

void gko_pool::setReportHandler(ReportHandler_t report_func)
{
    this->reportHandler = report_func;
}

gko_pool *gko_pool::getInstance()
{   if (! _instance)
    {
        pthread_mutex_lock(&instance_lock);
        if (!_instance)
        {
            _instance = new gko_pool();
        }
        pthread_mutex_unlock(&instance_lock);
    }
    return _instance;
}

int gko_pool::gko_run()
{

    if (port >= 0 && gko_async_server_base_init() < 0)
    {
        GKOLOG(FATAL, "gko_async_server_base_init failed");
        return -2;
    }

    if (conn_client_list_init() < 1)
    {
        GKOLOG(FATAL, "conn_client_list_init failed");
        return -3;
    }
    if (thread_init() != 0)
    {
        GKOLOG(FATAL, FLF("thread_init failed"));
        return -4;
    }

    if (sig_watcher(int_worker) != 0)
    {
        GKOLOG(FATAL, "signal watcher start error");//todo
        gko_quit(1);
    }

    return event_base_loop(g_ev_base, 0);
}

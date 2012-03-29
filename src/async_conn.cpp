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
/// FUNC DICT
char (* gko_pool::cmd_list_p)[CMD_LEN] = NULL;
///server func list
func_t * gko_pool::func_list_p = NULL;
/// cmd type conut
int gko_pool::cmd_count = 0;
/// init global lock
pthread_mutex_t gko_pool::instance_lock = PTHREAD_MUTEX_INITIALIZER;
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
    g_client_list = new struct conn_client *[option->connlimit];
    if (g_client_list == NULL)
    {
        gko_log(FATAL, "Malloc error, cannot init client pool");
        return -1;
    }
    memset(g_client_list, 0, option->connlimit * sizeof(struct conn_client *));
    g_total_clients = 0;

    gko_log(NOTICE, "Client pool initialized as %d", option->connlimit);

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
    g_server = new struct conn_server;
    if(! g_server)
    {
        gko_log(FATAL, "new for g_server failed");
        return -1;
    }
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
    g_server->on_data_callback = conn_send_data;
    g_server->is_server = this->port < 0 ? 0 : 1;
    /// A new TCP server
    int ret;
    while ((ret = conn_tcp_server(g_server)) < 0)
    {
        if (ret == BIND_FAIL)
        {
            if (g_server->srv_port < MIN_PORT || this->port >= 0)
            {
                gko_log(FATAL, "bind port failed, last try is %d", port);
                return -1;
            }
            g_server->srv_port --;
            usleep(BIND_INTERVAL);
        }
        else
        {
            gko_log(FATAL, "conn_tcp_server start error");
            return -1;
        }
    }

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
        gko_log(FATAL, "serv port %d > %d", g_server->srv_port, MAX_PORT);
        return -1;
    }

    /// If port number below 1024, root privilege needed
    if (g_server->srv_port < MIN_PORT)
    {
        /// CHECK ROOT PRIVILEGE
        if (ROOT != geteuid())
        {
            gko_log(FATAL, "Port %d number below 1024, root privilege needed",
                    g_server->srv_port);
            return -1;
        }
    }

    /// Create new socket
    g_server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server->listen_fd < 0)
    {
        gko_log(WARNING, "Socket creation failed");
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
            gko_log(FATAL, "Socket bind failed on port");
            return -1;
        }
        else
        {
            return BIND_FAIL;
        }
    }
    gko_log(NOTICE, "Upload port bind on port %d", g_server->srv_port);

    /// Listen socket
    if (listen(g_server->listen_fd, g_server->listen_queue_length) < 0)
    {
        gko_log(FATAL, "Socket listen failed");
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
        gko_log(WARNING, "Socket set non-blocking failed");
        return -1;
    }

    g_server->start_time = time((time_t *) NULL);

    ///gko_log(WARNING, "Socket server created on port %d", server->srv_port);
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
    ///struct conn_server *server = (struct conn_server *) arg;
    /// Accept new connection
    client_fd = accept(fd, (struct sockaddr *) &client_addr, &client_len);
    if (-1 == client_fd)
    {
        gko_log(WARNING, "Accept error");
        return;
    }
    /// Add connection to event queue
    client = gko_pool::getInstance()->add_new_conn_client(client_fd);
    if (!client)
    {
        ///close socket and further receives will be disallowed
        shutdown(client_fd, SHUT_RD);
        close(client_fd);
        gko_log(WARNING, "Server limited: I cannot serve more clients");
        return;
    }
    /// set blocking
    ///fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)& ~O_NONBLOCK);

    /// Try to set non-blocking
    if (setnonblock(client_fd) < 0)
    {
        gko_pool::getInstance()->conn_client_free(client);
        gko_log(FATAL, "Client socket set non-blocking error");
        return;
    }

    /// Client initialize
    client->client_addr = inet_addr(inet_ntoa(client_addr.sin_addr));
    client->client_port = client_addr.sin_port;
    client->conn_time = time((time_t *) NULL);
    gko_pool::getInstance()->thread_worker_dispatch(client->id);

    return;
}

/**
 * @brief Before send data all req to server will filtered by conn_send_data
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_pool::conn_send_data(int fd, void * arg, unsigned int len)
{
    int i;
    struct conn_client *client = (struct conn_client *) arg;
    char * p = ((char *) client) + CMD_PREFIX_BYTE;
    i = parse_req(p);
    if (i != 0)
    {
        gko_log(NOTICE, "got req: %s, index: %d", p, i);
    }
    else
    {
        gko_log(DEBUG, "got req: %s, index: %d", p, i);
    }
    (*func_list_p[i])(p, fd);
    ///gko_log(NOTICE, "read_buffer:%s", p);
    return;
}

/**
 * @brief Event on data from client
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_pool::conn_tcp_server_on_data(int fd, short ev, void *arg)
{
    struct conn_client *client = (struct conn_client *) arg;
    int res;
    unsigned int buffer_avail;
    int read_counter = 0;
    unsigned short proto_ver;
    int msg_len;
    char nouse_buf[CMD_PREFIX_BYTE];

    if (!client || !client->client_fd)
    {
        return;
    }

    if (fd != client->client_fd)
    {
        /// Sanity
        gko_pool::getInstance()->conn_client_free(client);
        return;
    }

    /// read the proto ver
    if (read(client->client_fd, &proto_ver, sizeof(proto_ver)) <= 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ///#define EAGAIN      35      /** Resource temporarily unavailable **/
            ///#define EWOULDBLOCK EAGAIN      /** Operation would block **/
            ///#define EINPROGRESS 36      /** Operation now in progress **/
            ///#define EALREADY    37      /** Operation already in progress **/
            //perror("Socket read error");
            gko_log(WARNING, "Socket read proto_ver error %d", errno);
            gko_pool::getInstance()->conn_client_free(client);
            return;
        }
    }
    if (proto_ver != PROTO_VER)
    {
        gko_log(WARNING, FLF("unsupported proto_ver"));
        return;
    }

    /// read the nouse_buf
    if (read(client->client_fd, nouse_buf,
            CMD_PREFIX_BYTE - sizeof(proto_ver) - sizeof(msg_len)) <= 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ///#define EAGAIN      35      /** Resource temporarily unavailable **/
            ///#define EWOULDBLOCK EAGAIN      /** Operation would block **/
            ///#define EINPROGRESS 36      /** Operation now in progress **/
            ///#define EALREADY    37      /** Operation already in progress **/
            //perror("Socket read error");
            gko_log(WARNING, "Socket read nouse_buf error %d", errno);
            gko_pool::getInstance()->conn_client_free(client);
            return;
        }
    }

    /// read the msg_len
    if (read(client->client_fd, &msg_len, sizeof(msg_len)) <= 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ///#define EAGAIN      35      /** Resource temporarily unavailable **/
            ///#define EWOULDBLOCK EAGAIN      /** Operation would block **/
            ///#define EINPROGRESS 36      /** Operation now in progress **/
            ///#define EALREADY    37      /** Operation already in progress **/
            //perror("Socket read error");
            gko_log(WARNING, "Socket read msg_len error %d", errno);
            gko_pool::getInstance()->conn_client_free(client);
            return;
        }
    }

    if (!client->read_buffer)
    {
        /// Initialize buffer
        client->read_buffer = new char[CLNT_READ_BUFFER_SIZE];
        if(! client->read_buffer)
        {
        	gko_log(FATAL, "new for client->read_buffer failed");
        	gko_pool::getInstance()->conn_client_free(client);
            return;
        }
        client->rbuf_size = buffer_avail = CLNT_READ_BUFFER_SIZE;
    }
    else
    {
        buffer_avail = client->rbuf_size;
    }

    while ((res = read(client->client_fd, client->read_buffer + read_counter,
            buffer_avail)) > 0)
    {
        ///gko_log(NOTICE, "res: %d",res);
        ///gko_log(NOTICE, "%s",client->read_buffer+read_counter);
        read_counter += res;
//        if ((unsigned int)res == buffer_avail)
//        {
//            client->buffer_size *= 2;
//            char * tmp = client->read_buffer;
//            client->read_buffer = new char[client->buffer_size];
//            if (client->read_buffer == NULL)
//            {
//                gko_log(FATAL, "realloc error");
//                conn_client_free(client);
//                return;
//            }
//            memcpy(client->read_buffer, tmp, read_counter);
//            delete [] tmp;
//        }
        buffer_avail = client->rbuf_size - read_counter;
        if (read_counter == msg_len)
        {
            *(client->read_buffer + read_counter) = '\0';
//            memset(client->read_buffer + read_counter, 0, buffer_avail);
            break;
        }
    }
    if (res < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ///#define EAGAIN      35      /** Resource temporarily unavailable **/
            ///#define EWOULDBLOCK EAGAIN      /** Operation would block **/
            ///#define EINPROGRESS 36      /** Operation now in progress **/
            ///#define EALREADY    37      /** Operation already in progress **/
            //perror("Socket read error");
            gko_log(WARNING, "Socket read error %d", errno);
            gko_pool::getInstance()->conn_client_free(client);
            return;
        }
    }
    ///gko_log(NOTICE, "read_buffer:%s", client->read_buffer);///test
    if (gko_pool::getInstance()->g_server->on_data_callback)
    {
        gko_pool::getInstance()->g_server->on_data_callback(client->client_fd,
                (void *) client->read_buffer, sizeof(client->read_buffer));
    }
    gko_pool::getInstance()->conn_client_free(client);

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
    gko_log(DEBUG, "add_new_conn_client id %d",id);///test
    if (id >= 0)
    {
        tmp = g_client_list[id];
        if (!tmp)
        {
            tmp = new struct conn_client;
            if(!tmp)
            {
            	gko_log(FATAL, "new conn_client failed");
            	return tmp;
            }
        }
    }
    else
    {
        /// FIXME
        /// Client list pool full, if you want to enlarge it,
        /// modify async_pool.h source please
        gko_log(WARNING, "Client list full");
        return tmp;
    }

    if (tmp)
    {
        memset(tmp, 0, sizeof(struct conn_client));
        conn_client_clear(tmp);
        tmp->id = id;
        tmp->client_fd = client_fd;
        g_client_list[id] = tmp;
        g_total_clients++;
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

    for (i = 0; i < option->connlimit; i++)
    {
        if (!g_client_list[i] || 0 == g_client_list[i]->conn_time)
        {
            return i;
        }
    }

    return -1;
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

        if (client->read_buffer)
        {
            free(client->read_buffer);
        }
        client->rbuf_size = RBUF_SZ;
        client->have_read = 0;
        client->need_read = CMD_PREFIX_BYTE;

        if (client->write_buffer)
        {
            free(client->write_buffer);
        }
        client->wbuf_size = WBUF_SZ;
        client->have_write = 0;
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
        g_curr_thread(0), port(pt)
{
    g_ev_base = (struct event_base*)event_init();
    if (!g_ev_base)
    {
        gko_log(FATAL, "event init failed");
    }
}

gko_pool::gko_pool()
    :
        g_curr_thread(0)

{
    g_ev_base = (struct event_base*)event_init();
    if (!g_ev_base)
    {
        gko_log(FATAL, "event init failed");
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

void gko_pool::setFuncTable(char(*cmd_list)[CMD_LEN], func_t * func_list,
        int cmdcount)
{
    cmd_list_p = cmd_list;
    func_list_p = func_list;
    cmd_count = cmdcount;
}

/**
 * @brief close conn, shutdown && close
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gko_pool::conn_close()
{

    for (int i = 0; i < option->connlimit; i++)
    {
        conn_client_free(g_client_list[i]);
    }

    //shutdown(g_server->listen_fd, SHUT_RDWR);
    close(g_server->listen_fd);
    memset(g_server, 0, sizeof(struct conn_server));

    return 0;
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

    if (gko_async_server_base_init() < 0)
    {
        gko_log(FATAL, "gko_async_server_base_init failed");
        return -2;
    }

    if (conn_client_list_init() < 1)
    {
        gko_log(FATAL, "conn_client_list_init failed");
        return -3;
    }
    if (thread_init() != 0)
    {
        gko_log(FATAL, FLF("thread_init failed"));
        return -4;
    }

    return event_base_loop(g_ev_base, 0);
}

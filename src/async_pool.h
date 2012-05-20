/**
 *  async_pool.h
 *  gingko
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 *
 **/
#ifndef ASYNC_POOL_H_
#define ASYNC_POOL_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/resource.h>
#include <pthread.h>
#include <errno.h>

#include "event.h"

#include "gko_errno.h"

static const int        RBUF_SZ =          (2 * 1024);
static const int        WBUF_SZ =          (4 * 1024);

/// Thread worker
class thread_worker
{
public:
    int id;
    pthread_t tid;
    struct event_base *ev_base;
    struct event ev_notify;
    struct event ev_cleantimeout;
    int notify_recv_fd;
    int notify_send_fd;
    std::set<int> conn_set;

    /// put conn into current thread conn_set
    void add_conn(int c_id);
    /// del conn from current thread conn_set
    void del_conn(int c_id);
};

enum conn_states {
    conn_nouse = 0,
    conn_listening,     /**< now the main schedule thread do the accept */
    conn_waiting,       /**< waiting for a readable socket */
    conn_read,          /**< reading the cmd header */
    conn_nread,         /**< reading in a fixed number of bytes */
    conn_parse_header,  /**< try to parse the cmd header */
    conn_parse_cmd,     /**< try to parse a command from the input buffer */
    conn_write,         /**< writing out a simple response */
    conn_mwrite,        /**< writing out many items sequentially */
    conn_closing,       /**< closing this connection */

    /// for client side
    conn_connecting = 101,
    conn_connected,
    conn_timeout,
    conn_reseted,
    conn_connect_fail,
    conn_closed,

    conn_max_state      /**< Max state value (used for assertion) */
};

enum conn_type {
    coming_conn = 0, /// this is default
    active_conn = 1
};

/// Connection client
struct conn_client
{
    int id;
    int worker_id;
    int client_fd;
    long task_id;
    long sub_task_id;
    in_addr_t client_addr;
    int client_port;
    time_t conn_time;
    func_t handle_client;
    struct event event;
    enum conn_states state;
    enum error_no err_no;
    enum conn_type type;
    int ev_flags;

    char *read_buffer;
    unsigned int rbuf_size;
    unsigned int need_read;
    unsigned int have_read;

    char *__write_buffer;
    char *write_buffer;
    unsigned int wbuf_size;
    unsigned int __need_write;
    unsigned int need_write;
    unsigned int have_write;

};

/// Connection server struct
struct conn_server
{
    char is_server;
    int listen_fd;
    struct sockaddr_in listen_addr;
    in_addr_t srv_addr;
    int srv_port;
    unsigned int start_time;
    int nonblock;
    int listen_queue_length;
    int tcp_send_buffer_size;
    int tcp_recv_buffer_size;
    int send_timeout;
    int tcp_reuse;
    int tcp_nodelay;
    struct event ev_accept;
    //void *(* on_data_callback)(void *);
    ProcessHandler_t on_data_callback;
};

enum aread_result {
    READ_DATA_RECEIVED,
    READ_HEADER_RECEIVED,
    READ_NEED_MORE,
    READ_NO_DATA_RECEIVED,
    READ_ERROR,            /** an error occured (on the socket) (or client closed connection) */
    READ_MEMORY_ERROR      /** failed to allocate more memory */
};

enum awrite_result {
    WRITE_DATA_SENT,
    WRITE_HEADER_SENT,
    WRITE_SENT_MORE,
    WRITE_NO_DATA_SENT,
    WRITE_ERROR            /** an error occured (on the socket) (or client closed connection) */
};

class gko_pool
{
private:
    static gko_pool * _instance;
    /// global lock
    static pthread_mutex_t instance_lock;
    static pthread_mutex_t conn_list_lock;
    static pthread_mutex_t thread_list_lock;

    int g_curr_thread;
    int g_curr_conn;

    thread_worker ** g_worker_list;
    struct event_base *g_ev_base;
    int g_total_clients;
    int g_total_connect;
    struct conn_client **g_client_list;
    struct conn_server *g_server;
    int port;
    s_option_t * option;
    ProcessHandler_t pHandler;
    ReportHandler_t reportHandler;

    static void conn_send_data(void *c);
    /// Accept new connection
    static void conn_tcp_server_accept(int fd, short ev, void *arg);
    /// Close conn, shutdown && close
    static void * thread_worker_init(void *arg);
    /// Close conn, shutdown && close
    static void thread_worker_process(int fd, short ev, void *arg);
    /// Connection buffer init
    static void conn_buffer_init(conn_client *client);
    /// ReAdd the event
    static bool update_event(conn_client *c, const int new_flags);
    /// State updater
    static void conn_set_state(conn_client *c, enum conn_states state);
    /// Event handler for worker thread
    static void worker_event_handler(const int fd, const short which, void *arg);
    /// Event handler for clean up timeout conn
    static void clean_handler(const int fd, const short which, void *arg);
    /// Async read
    static enum aread_result aread(conn_client *c);
    /// Async write
    static enum awrite_result awrite(conn_client *c);
    /// The drive machine of memcached
    static void state_machine(conn_client *c);

    /// non-blocking version connect
    int nb_connect(const s_host_t * h, struct conn_client* conn);
    int connect_hosts(const std::vector<s_host_t> & host_vec,
     std::vector<struct conn_client> * conn_vec);
    int disconnect_hosts(std::vector<struct conn_client> & conn_vec);


    int clean_conn_timeout(thread_worker *worker, time_t now);
    int thread_worker_new(int id);
    int thread_list_find_next(void);
    int conn_client_list_init(void);
    int gko_async_server_base_init(void);
    /// Accept new connection, start listen etc.
    int conn_tcp_server(struct conn_server *c);
    /// Accept new connection
    struct conn_client * add_new_conn_client(int client_fd);
    /// Event on data from client
    int conn_client_list_find_free();
    /// clear client struct
    int conn_client_clear(struct conn_client *client);
    /// clear the "session"
    int conn_client_free(struct conn_client *client);
    /// Get client object from pool by given client_id
    struct conn_client * conn_client_list_get(int id);
    /// Dispatch to worker
    void thread_worker_dispatch(int sig_id);
    /// init the whole thread pool
    int thread_init();
    /// construct func
    gko_pool(const int pt);
    /// another construct func
    gko_pool();

public:
    static s_host_t gko_serv;

    static gko_pool *getInstance();

    void setProcessHandler(ProcessHandler_t func_list);
    void setReportHandler(ReportHandler_t report_func);
    int getPort() const;
    void setPort(int port);
    s_option_t *getOption() const;
    void setOption(s_option_t *option);

    /// global run func
    int gko_run();
    int gko_loopexit(int timeout);

    int make_active_connect(const char * host, const int port, const long task_id, const long sub_task_id, const char * cmd);

};

#endif /** ASYNC_POOL_H_ **/

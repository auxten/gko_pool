/*
 * client.cpp
 *
 *  Created on: Feb 27, 2012
 *      Author: auxten
 */

#include "gko.h"

/// gingko global stuff
s_gingko_global_t gko;
const int T_NUM = 100;
const int CMD_CNT = 100;

const s_host_t server =
    {
        "127.0.0.1",
        2120
    };

void * send_test(void *)
{
    char msg[MSG_LEN] =
        "TEST\tdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
    for (int i = 0; i < CMD_CNT; i++)
    {
        if (chat_with_host(&server, msg, 2, 2) < 0)
        {
            GKOLOG(FATAL, "sending test message failed");
        }
    }
//    GKOLOG(DEBUG, "%s", msg);
    pthread_exit(NULL);
}

void * send_scmd(void *)
{
    char msg[MSG_LEN] =
        "SCMD\tlocalhost\tdf -h";
    for (int i = 0; i < CMD_CNT; i++)
    {
        if (chat_with_host(&server, msg, 2, 2) < 0)
        {
            GKOLOG(FATAL, "sending test message failed");
        }
    }
//    GKOLOG(DEBUG, "%s", msg);
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    gko.opt.to_debug = 1;
    pthread_attr_t g_attr;
    pthread_t vnode_pthread[T_NUM];
    void *status;
    unsigned short proto_ver;
    int msg_len;

    //    char buf[20] =
    //        { '\0' };
    //    fill_cmd_head(buf, INT32_MAX);
    //    parse_cmd_head(buf, &proto_ver, &msg_len);
    //    GKOLOG(DEBUG, "%s %d", buf, msg_len);
    //    std::vector<struct conn_client> conn_vec;
    //    std::vector<s_host_t> srv_vec(1000, server);
    //    connect_hosts(srv_vec, &conn_vec);
    //    sleep(10);
    //    disconnect_hosts(conn_vec);

//    char * array[5] = {NULL, NULL, NULL, NULL, NULL};
//    char t1[100] = "aa\t\t\tddd\tbbb\t\tccc\trrr\tyyy";
//    sep_arg(t1, array, 5);
//    GKOLOG(DEBUG, "%s, %s, %s, %s, %s", array[0], array[1], array[2], array[3], array[4]);
//    GKOLOG(DEBUG, "for test %s %d", "ddd", 10);
//    return 0;

    if (pthread_attr_init(&g_attr) != 0)
    {
        GKOLOG(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_attr_setdetachstate(&g_attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        GKOLOG(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }

    for (int i = 0; i < T_NUM; i++)
    {
        if (pthread_create(&vnode_pthread[i], &g_attr, send_scmd, NULL))
        {
            GKOLOG(FATAL, "download thread %d create error", i);
            return -1;
        }
    }
    for (int i = 0; i < T_NUM; i++)
    {
        if (pthread_join(vnode_pthread[i], &status))
        {
            GKOLOG(FATAL, "download thread %d join error", i);
            return -1;
        }
        if (status != (void *) 0)
        {
            GKOLOG(FATAL, "thread %d joined with error num %lld", i,
                    (long long) status);
            return -1;
        }
    }

    return 0;
}


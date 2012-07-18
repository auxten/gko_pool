/**
 * socket.cpp
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 **/

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>

#include "config.h"
#include "gingko.h"
#include "log.h"
#include "socket.h"
#ifdef __APPLE__
#include <poll.h>
#else
#include <sys/poll.h>
#endif /** __APPLE__ **/

char * addr_itoa(in_addr_t address, char * str)
{
    u_int32_t addr = (u_int32_t)address;
    snprintf(str, 16,"%u.%u.%u.%u", (addr) & 255u, (addr >> 8) & 255u, (addr >> 16) & 255u, (addr >> 24) & 255u);
    return str;
}

/**
 * @brief Set non-blocking
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int setnonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        return flags;
    }

    if (!(flags & O_NONBLOCK))
    {
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) < 0)
        {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Set blocking
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int setblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        return flags;
    }

    if (flags & O_NONBLOCK)
    {
        flags &= ~O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) < 0)
        {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief connect to a host
 *
 * @see
 * @note
 *     h: pointer to s_host_t
 *     recv_sec: receive timeout seconds, 0 for never timeout
 *     return the socket when succ
 *     return < 0 when error, specially HOST_DOWN_FAIL indicate host dead
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int connect_host(const s_host_t * h, const int recv_sec, const int send_sec)
{
    int sock = -1;
    int ret;
    int select_ret;
    int res;
    socklen_t res_size = sizeof res;
    struct sockaddr_in channel;
    in_addr_t host;
    int addr_len;
    struct timeval recv_timeout;
    struct timeval send_timeout;
#if HAVE_POLL
#else
    fd_set wset;
#endif /* HAVE_POLL */

    addr_len = getaddr_my(h->addr, &host);
    if (FAIL_CHECK(!addr_len))
    {
        GKOLOG(WARNING, "gethostbyname %s error", h->addr);
        ret = -1;
        goto CONNECT_END;
    }
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (FAIL_CHECK(sock < 0))
    {
        GKOLOG(WARNING, "get socket error");
        ret = -1;
        goto CONNECT_END;
    }

    recv_timeout.tv_usec = 0;
    recv_timeout.tv_sec = recv_sec ? recv_sec : RCV_TIMEOUT;
    send_timeout.tv_usec = 0;
    send_timeout.tv_sec = send_sec ? send_sec : SND_TIMEOUT;

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, &host, addr_len);
    channel.sin_port = htons(h->port);

    /** set the connect non-blocking then blocking for add timeout on connect **/
    if (FAIL_CHECK(setnonblock(sock) < 0))
    {
        GKOLOG(WARNING, "set socket non-blocking error");
        ret = -1;
        goto CONNECT_END;
    }

    /** connect and send the msg **/
    if (FAIL_CHECK(connect(sock, (struct sockaddr *) &channel, sizeof(channel)) &&
            errno != EINPROGRESS))
    {
        GKOLOG(WARNING, "connect error");
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }

    /** Wait for write bit to be set **/
#if HAVE_POLL
    {
        struct pollfd pollfd;

        pollfd.fd = sock;
        pollfd.events = POLLOUT;

        /* send_sec is in seconds, timeout in ms */
        select_ret = poll(&pollfd, 1, (int)(send_sec * 1000 + 1));
    }
#else
    {
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        select_ret = select(sock + 1, 0, &wset, 0, &send_timeout);
    }
#endif /* HAVE_POLL */
    if (select_ret < 0)
    {
        GKOLOG(WARNING, "select/poll error on connect");
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }
    if (!select_ret)
    {
        GKOLOG(WARNING, "connect timeout on connect");
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }

    /**
     * check if connection is RESETed, maybe this is the
     * best way to do that
     * SEE: http://cr.yp.to/docs/connect.html
     **/
    (void) getsockopt(sock, SOL_SOCKET, SO_ERROR, &res, &res_size);
    if (CONNECT_DEST_DOWN(res))
    {
//        GKOLOG(NOTICE, "connect dest is down errno: %d", res);
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }

    ///GKOLOG(WARNING, "selected %d ret %d, time %d", sock, select_ret, send_timeout.tv_sec);
    /** set back blocking **/
    if (FAIL_CHECK(setblock(sock) < 0))
    {
        GKOLOG(WARNING, "set socket non-blocking error");
        ret = -1;
        goto CONNECT_END;
    }

    /** set recv & send timeout **/
    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &recv_timeout,
                    sizeof(struct timeval))))
    {
        GKOLOG(WARNING, "setsockopt SO_RCVTIMEO error");
        ret = -1;
        goto CONNECT_END;
    }
    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &send_timeout,
                    sizeof(struct timeval))))
    {
        GKOLOG(WARNING, "setsockopt SO_SNDTIMEO error");
        ret = -1;
        goto CONNECT_END;
    }

    ret = sock;

    CONNECT_END:
    ///
    if (ret < 0 && sock >= 0)
    {
        close_socket(sock);
    }
    return ret;
}

/**
 * @brief gracefully close a socket, for client side
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int close_socket(int sock)
{
    ///  if (shutdown(sock, 2)) {
    ///      GKOLOG(WARNING, "shutdown sock error");
    ///      return -1;
    ///  }
//    struct linger so_linger;
//    so_linger.l_onoff = 1; /// close right now, no time_wait at serv
//    so_linger.l_linger = 0; /// at most wait for 1s
//    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger))))
//    {
//        GKOLOG(WARNING, "set so_linger failed");
//    }
    if (sock < 0)
        return 0;
    if (FAIL_CHECK(close(sock)))
    {
        GKOLOG(WARNING, "close sock error");
        return -1;
    }
    return 0;
}


/*
 * aysnc_clnt.h
 *
 *  Created on: May 4, 2012
 *      Author: auxten
 */

#ifndef AYSNC_CLNT_H_
#define AYSNC_CLNT_H_

/**
 * @brief non-blocking version connect
 *
 * @see
 * @note
 * @author auxten <auxtenwpc@gmail.com>
 * @date Apr 22, 2012
 **/
int nb_connect(const s_host_t * h, struct conn_client* conn);

int connect_hosts(const std::vector<s_host_t> & host_vec,
        std::vector<struct conn_client> * conn_vec);

int disconnect_hosts(std::vector<struct conn_client> & conn_vec);

#endif /* AYSNC_CLNT_H_ */

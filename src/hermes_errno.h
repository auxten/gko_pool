/*
 * gko_errno.h
 *
 *  Created on: May 17, 2012
 *      Author: auxten
 */

#ifndef GKO_ERRNO_H_
#define GKO_ERRNO_H_

enum error_no {
//////////////////// DO NOT USE DIRECTLY below //////////////////////
    /// succ or fail
    INVILID                 = -1,
    SUCC                    = 0,
    FAIL                    = 1,

    /// 失败原因
    ERROR                   = 10, /// 其它各种失败
    RECV_TIMEOUT            = 20,
    SEND_TIMEOUT            = 30,
    RESETED                 = 40,
    RECV_ERROR              = 50,
    SEND_ERROR              = 60,
    AGENT_ERROR             = 70,

    /// 任务阶段
    TODO                    = 100,
    DISPATCH                = 200,
    EXECUTE                 = 300,
    GKO_MYSQL               = 400,
    SERVER_INTERNAL         = 500,
//////////////////// DO  NOT USE DIRECTLY above //////////////////////
    ////以上不要直接使用

    /// 任务分发结果
    DISPATCH_SUCC           = DISPATCH + SUCC,
    DISPATCH_SEND_TIMEOUT   = DISPATCH + SEND_TIMEOUT + FAIL, /// 网络发送超时
    DISPATCH_RECV_TIMEOUT   = DISPATCH + RECV_TIMEOUT + FAIL, /// 发送任务获取agent收到确认超时
    DISPATCH_SEND_ERROR     = DISPATCH + SEND_ERROR + FAIL, /// 网络连接错误,eg. reset
    DISPATCH_RECV_ERROR     = DISPATCH + RECV_ERROR + FAIL, /// 网络连接错误,eg. reset
    DISPATCH_AGENT_FAIL     = DISPATCH + AGENT_ERROR + FAIL, /// agent接受任务后立刻返回失败

    /// 任务执行结果
    EXECUTE_SUCC            = EXECUTE + SUCC, ///
    EXECUTE_FAIL            = EXECUTE + FAIL, /// agent执行失败

    /// 服务器内部状态
    SERVER_INTERNAL_ERROR   = SERVER_INTERNAL + ERROR + FAIL,

};
#endif /* GKO_ERRNO_H_ */

/**
 * log.h
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 *
 **/

#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

///fatal
static const u_int8_t FATAL =     0;
///warning
static const u_int8_t WARNING =   1;
///notice
static const u_int8_t NOTICE =    2;
///trace
static const u_int8_t TRACE =     3;
///debug
static const u_int8_t DEBUG =     4;

/// to print the file line func
///  MUST NOT use it in "const char *fmt, ..." type func
//#define FLF(a)  "{%s:%d %s} %s",__FILE__,__LINE__,__func__,#a
#define FLF(a) a

#define GKOLOG(level, args...)  gko_log_flf(level, __FILE__, __LINE__, __func__, args)

/// log handler
void gko_log_flf(const u_int8_t log_level, const char *file, const int line, const char *func, const char *fmt, ...);
int lock_log(void);

#endif /** LOG_H_ **/

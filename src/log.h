/**
 * log.h
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 *
 **/

#ifndef LOG_H_
#define LOG_H_

///fatal
static const u_char FATAL =     0;
///warning
static const u_char WARNING =   1;
///notice
static const u_char NOTICE =    2;
///trace
static const u_char TRACE =     3;
///debug
static const u_char DEBUG =     4;

/// to print the file line func
///  MUST NOT use it in "const char *fmt, ..." type func
#define FLF(a)  "{%s:%d %s} %s",__FILE__,__LINE__,__func__,#a

/// log handler
void gko_log(const u_char log_level, const char *fmt, ...);
int lock_log(void);

#endif /** LOG_H_ **/

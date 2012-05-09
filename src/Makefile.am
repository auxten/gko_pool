SUBDIRS = hash .
INCLUDES = -I$(srcdir)/src\
    -I$(srcdir)/hash \
    -include config.h \
    -I$(srcdir)/../lib/libev/include 
#    -I$(srcdir)/../../../../../../third-64/libev/include 
#    -I$(srcdir)/../../../../../../../third-64/libev/include 
export INCLUDES

#noinst_PROGRAMS=gingko_serv gingko_clnt serv_unittest clnt_unittest
noinst_LIBRARIES=libgko.a

DEFS+=-O0 -D_GNU_SOURCE -ggdb -funsigned-char
libgko_a_SOURCES=async_threads.cpp async_conn.cpp async_clnt.cpp \
    gingko_base.cpp log.cpp socket.cpp limit.cpp
libgko_a_LDFLAGS=-O0 -D_GNU_SOURCE -ggdb -funsigned-char
INCLUDES = \
	-I$(srcdir)/src\
    -include config.h
#    -I$(srcdir)/../lib/libev/include \
#    -I$(srcdir)/../lib/c-ares/include 
#    -I$(srcdir)/../../../../../../third-64/libev/include 
#    -I$(srcdir)/../../../../../../../third-64/libev/include 
export INCLUDES

#noinst_PROGRAMS=gingko_serv gingko_clnt serv_unittest clnt_unittest
lib_LIBRARIES=libgko.a

libgko_a_includedir=$(includedir)/
libgko_a_include_HEADERS = \
	$(srcdir)/gko.h \
	$(srcdir)/gingko.h \
	$(srcdir)/async_pool.h \
	$(srcdir)/log.h \
	$(srcdir)/socket.h \
	$(srcdir)/memory.h \
	$(srcdir)/dict.h \
	$(srcdir)/gko_errno.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgko.pc


DEFS+=-D_GNU_SOURCE -ggdb -funsigned-char -fPIC -DPIC
libgko_a_SOURCES=async_threads.cpp async_conn.cpp async_clnt.cpp async_dns.cpp \
    gingko_base.cpp log.cpp socket.cpp limit.cpp memory.cpp dict.c
#libgko_a_LDFLAGS=-D_GNU_SOURCE -ggdb -funsigned-char
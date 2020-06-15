#
ACLOCAL_AMFLAGS = -I m4
lib_LTLIBRARIES = libblocklist.la
include_HEADERS = ../include/blocklist.h

bin_PROGRAMS = blocklistd blocklistctl srvtest cltest

VPATH = ../bin:../lib:../test:../include

AM_CPPFLAGS = -I../include  -DDOT="."
AM_CFLAGS = @WARNINGS@

libblocklist_la_SOURCES = bl.c blocklist.c
libblocklist_la_LDFLAGS = -no-undefined -version-info 0:0:0
libblocklist_la_LIBADD = $(LTLIBOBJS)

SRCS = internal.c support.c run.c conf.c state.c
blocklistd_SOURCES = blocklistd.c ${SRCS}
blocklistd_LDADD = libblocklist.la
blocklistctl_SOURCES = blocklistctl.c ${SRCS}
blocklistctl_LDADD = libblocklist.la
srvtest_SOURCES = srvtest.c ${SRCS}
srvtest_LDADD = libblocklist.la
cltest_SOURCES = cltest.c ${SRCS}
cltest_LDADD = libblocklist.la

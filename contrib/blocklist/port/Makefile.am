#
ACLOCAL_AMFLAGS = -I m4
lib_LTLIBRARIES = libblocklist.la
include_HEADERS = $(srcdir)/../include/blocklist.h

exampledir = $(datarootdir)/examples
example_DATA = $(srcdir)/../etc/blocklistd.conf $(srcdir)/../etc/npf.conf $(srcdir)/../etc/ipf.conf

sbin_PROGRAMS = blocklistd blocklistctl
noinst_PROGRAMS = srvtest cltest
libexec_SCRIPTS = $(srcdir)/../libexec/blocklistd-helper

man5_MANS = $(srcdir)/../bin/blocklistd.conf.5
man8_MANS = $(srcdir)/../bin/blocklistd.8 $(srcdir)/../bin/blocklistctl.8

VPATH = $(srcdir)/../port:$(srcdir)/../bin:$(srcdir)/../lib:$(srcdir)/../test:$(srcdir)/../include

AM_CPPFLAGS = -I$(srcdir)/../include  -DDOT="."
AM_CPPFLAGS += -D_PATH_BLCONF=\"$(sysconfdir)/blocklistd.conf\"
AM_CPPFLAGS += -D_PATH_BLCONTROL=\"$(libexecdir)/blocklistd-helper\"
AM_CPPFLAGS += -D_PATH_BLSOCK=\"$(runstatedir)/blocklistd.sock\"
AM_CPPFLAGS += -D_PATH_BLSTATE=\"$(localstatedir)/db/blocklistd.db\"
AM_CPPFLAGS += -std=c99 -D_POSIX_C_SOURCE=200809L -D__EXTENSIONS__
AM_CPPFLAGS += -D__BSD_VISIBLE=1
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

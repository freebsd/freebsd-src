# $Id: Makefile.am 22281 2007-12-13 20:35:52Z lha $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4)

bin_PROGRAMS = rcp

rcp_SOURCES  = rcp.c util.c rcp_locl.h extern.h

man_MANS = rcp.1

EXTRA_DIST = $(man_MANS)

LDADD = $(LIB_roken)

# $Id: Makefile.am,v 1.14 2000/02/07 03:13:00 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

bin_PROGRAMS = rsh

libexec_PROGRAMS = rshd

rsh_SOURCES  = rsh.c common.c rsh_locl.h

rshd_SOURCES = rshd.c common.c rsh_locl.h

LDADD = $(LIB_kafs) \
	$(LIB_krb5) \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_roken)

# $Id: Makefile.am,v 1.1 1999/07/22 11:36:26 assar Exp $

include $(top_srcdir)/Makefile.am.common

noinst_PROGRAMS = kf kfd

kf_SOURCES = kf.c kf_locl.h

kfd_SOURCES = kfd.c kf_locl.h

LDADD = $(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

# $Id: Makefile.am,v 1.11 1999/04/20 16:48:34 assar Exp $

include $(top_srcdir)/Makefile.am.common

man_MANS = kpasswd.1 kpasswdd.8

bin_PROGRAMS = kpasswd

kpasswd_SOURCES = kpasswd.c kpasswd_locl.h

libexec_PROGRAMS = kpasswdd

kpasswdd_SOURCES = kpasswdd.c kpasswd_locl.h

kpasswdd_LDADD = \
	$(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(LDADD) \
	$(LIB_dlopen) \
	$(DBLIB)

LDADD = $(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

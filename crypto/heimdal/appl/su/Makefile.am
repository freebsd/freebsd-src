# $Id: Makefile.am,v 1.6 2000/11/15 22:51:10 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

bin_PROGRAMS = su
bin_SUIDS = su
su_SOURCES = su.c

LDADD = $(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

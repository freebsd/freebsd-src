# $Id: Makefile.am 21986 2007-10-19 05:22:57Z lha $
# $FreeBSD: src/crypto/heimdal/appl/su/Makefile.am,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4) $(INCLUDE_hcrypto)

bin_PROGRAMS = su
bin_SUIDS = su
su_SOURCES = su.c supaths.h
man_MANS = su.1

LDADD = $(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

EXTRA_DIST = $(man_MANS)

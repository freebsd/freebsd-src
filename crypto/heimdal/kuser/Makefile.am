# $Id: Makefile.am,v 1.27 2000/11/15 22:51:13 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

man_MANS = kinit.1 klist.1 kdestroy.1 kgetcred.1

bin_PROGRAMS = kinit kauth klist kdestroy kgetcred

kinit_SOURCES = kinit.c kinit_options.c

kauth_SOURCES = kinit.c kauth_options.c

noinst_PROGRAMS = kverify kdecode_ticket generate-requests

CHECK_LOCAL = $(bin_PROGRAMS)

kauth_LDADD = \
	$(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

kinit_LDADD	= $(kauth_LDADD)

kdestroy_LDADD	= $(kauth_LDADD)

klist_LDADD	= $(kauth_LDADD)

LDADD = \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

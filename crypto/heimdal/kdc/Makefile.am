# $Id: Makefile.am,v 1.33 1999/05/13 23:32:35 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

bin_PROGRAMS = string2key

sbin_PROGRAMS = kstash

libexec_PROGRAMS = hprop hpropd kdc

man_MANS = kdc.8 kstash.8 hprop.8 hpropd.8

hprop_SOURCES = hprop.c hprop-common.c hprop.h kadb.h 
hpropd_SOURCES = hpropd.c hprop-common.c hprop.h

kstash_SOURCES = kstash.c headers.h

string2key_SOURCES = string2key.c headers.h

kdc_SOURCES = \
	524.c		\
	config.c	\
	connect.c	\
	kaserver.c	\
	kdc_locl.h	\
	kerberos4.c	\
	kerberos4.h	\
	kerberos5.c	\
	log.c		\
	main.c		\
	misc.c		\
	rx.h


hprop_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb) $(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB) 

hpropd_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb) $(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB) 

LDADD = $(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB)


# $Id: Makefile.am,v 1.44 2003/01/14 05:47:06 lha Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(INCLUDE_des) -I$(srcdir)/../lib/krb5

bin_PROGRAMS = string2key

sbin_PROGRAMS = kstash

libexec_PROGRAMS = hprop hpropd kdc

man_MANS = kdc.8 kstash.8 hprop.8 hpropd.8 string2key.8

hprop_SOURCES = hprop.c mit_dump.c v4_dump.c hprop.h kadb.h 
hpropd_SOURCES = hpropd.c hprop.h

kstash_SOURCES = kstash.c headers.h

string2key_SOURCES = string2key.c headers.h

if KRB4
krb4_sources = kaserver.c rx.h
else
krb4_sources =
endif

kdc_SOURCES = \
	config.c	\
	connect.c	\
	kdc_locl.h	\
	kerberos5.c	\
	log.c		\
	main.c		\
	misc.c		\
	524.c		\
	kerberos4.c	\
	$(krb4_sources)


hprop_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(LIB_openldap) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb) $(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB) 

hpropd_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(LIB_openldap) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb) $(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB) 

LDADD = $(top_builddir)/lib/hdb/libhdb.la \
	$(LIB_openldap) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB)

kdc_LDADD = $(LDADD) $(LIB_pidfile)


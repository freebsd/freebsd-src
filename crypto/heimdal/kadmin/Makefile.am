# $Id: Makefile.am,v 1.25 2000/01/06 08:04:13 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_readline) $(INCLUDE_krb4)

sbin_PROGRAMS = kadmin

libexec_PROGRAMS = kadmind

kadmin_SOURCES =				\
	ank.c					\
	cpw.c					\
	del.c					\
	del_enctype.c				\
	dump.c					\
	ext.c					\
	get.c					\
	init.c					\
	kadmin.c				\
	load.c					\
	mod.c					\
	rename.c				\
	util.c					\
	random_password.c			\
	kadmin_locl.h

if KRB4
KRB4LIB = $(LIB_krb4)
version4_c = version4.c
endif

kadmind_SOURCES = kadmind.c server.c kadmin_locl.h $(version4_c)

EXTRA_kadmind_SOURCES = version4.c

COMMON_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB)

kadmind_LDADD = $(KRB4LIB) $(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(COMMON_LDADD) \
	$(LIB_dlopen)

kadmin_LDADD = \
	$(top_builddir)/lib/kadm5/libkadm5clnt.la \
	$(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(top_builddir)/lib/sl/libsl.la \
	$(LIB_readline) \
	$(COMMON_LDADD) \
	$(LIB_dlopen)

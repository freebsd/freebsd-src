# $Id: Makefile.am,v 1.32 2000/11/15 22:51:12 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_readline) $(INCLUDE_krb4) -I$(srcdir)/../lib/krb5

sbin_PROGRAMS = kadmin

libexec_PROGRAMS = kadmind

man_MANS = kadmin.8 kadmind.8

noinst_PROGRAMS = add_random_users

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

kadmind_SOURCES =				\
	kadmind.c				\
	server.c				\
	kadmin_locl.h				\
	$(version4_c)				\
	kadm_conn.c

EXTRA_kadmind_SOURCES = version4.c

add_random_users_SOURCES = add-random-users.c

COMMON_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(LIB_openldap) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DBLIB)

kadmind_LDADD = $(KRB4LIB) $(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(COMMON_LDADD) \
	$(LIB_pidfile) \
	$(LIB_dlopen)

kadmin_LDADD = \
	$(top_builddir)/lib/kadm5/libkadm5clnt.la \
	$(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(top_builddir)/lib/sl/libsl.la \
	$(LIB_readline) \
	$(COMMON_LDADD) \
	$(LIB_dlopen)

add_random_users_LDADD = \
	$(top_builddir)/lib/kadm5/libkadm5clnt.la \
	$(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(COMMON_LDADD) \
	$(LIB_dlopen)

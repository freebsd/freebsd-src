# $Id: Makefile.am,v 1.16 1999/10/30 08:51:45 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

bin_PROGRAMS = login

login_SOURCES = \
		login.c				\
		osfc2.c				\
		read_string.c			\
		utmp_login.c			\
		utmpx_login.c			\
		tty.c				\
		stty_default.c			\
		login_access.c			\
		login_locl.h			\
		login_proto.h			\
		conf.c				\
		shadow.c

LDADD = $(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(LIB_security)

$(srcdir)/login_protos.h:
	cd $(srcdir); perl ../../cf/make-proto.pl -o login_protos.h $(login_SOURCES) || rm -f login_protos.h

$(login_OBJECTS): $(srcdir)/login_protos.h

# $Id: Makefile.am,v 1.19 2000/11/15 22:51:08 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

bin_PROGRAMS = login

login_SOURCES =					\
		conf.c				\
		env.c				\
		login.c				\
		login_access.c			\
		login_locl.h			\
		login_protos.h			\
		osfc2.c				\
		read_string.c			\
		shadow.c			\
		stty_default.c			\
		tty.c				\
		utmp_login.c			\
		utmpx_login.c

LDADD = $(LIB_otp) \
	$(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(LIB_security) \
	$(DBLIB)

$(srcdir)/login_protos.h:
	cd $(srcdir); perl ../../cf/make-proto.pl -o login_protos.h $(login_SOURCES) || rm -f login_protos.h

$(login_OBJECTS): $(srcdir)/login_protos.h

# $Id: Makefile.am,v 1.14 2000/11/15 22:51:07 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I$(srcdir)/../common $(INCLUDE_readline) $(INCLUDE_krb4)

bin_PROGRAMS = ftp

CHECK_LOCAL = 

if KRB4
krb4_sources = krb4.c kauth.c
endif
if KRB5
krb5_sources = gssapi.c
endif

ftp_SOURCES = \
	cmds.c \
	cmdtab.c \
	extern.h \
	ftp.c \
	ftp_locl.h \
	ftp_var.h \
	main.c \
	pathnames.h \
	ruserpass.c \
	domacro.c \
	globals.c \
	security.c \
	security.h \
	$(krb4_sources) \
	$(krb5_sources)

EXTRA_ftp_SOURCES = krb4.c kauth.c gssapi.c

man_MANS = ftp.1

LDADD = \
	../common/libcommon.a \
	$(LIB_gssapi) \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_des) \
	$(LIB_roken) \
	$(LIB_readline)

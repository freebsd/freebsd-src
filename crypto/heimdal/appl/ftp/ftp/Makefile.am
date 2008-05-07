# $Id: Makefile.am 20466 2007-04-20 08:29:05Z lha $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += -I$(srcdir)/../common $(INCLUDE_readline) $(INCLUDE_krb4) $(INCLUDE_hcrypto)

bin_PROGRAMS = ftp

CHECK_LOCAL = 

if KRB4
krb4_sources = krb4.c
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
	kauth.c \
	$(krb4_sources) \
	$(krb5_sources)

EXTRA_ftp_SOURCES = krb4.c gssapi.c

man_MANS = ftp.1

LDADD = \
	../common/libcommon.a \
	$(LIB_gssapi) \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_hcrypto) \
	$(LIB_roken) \
	$(LIB_readline)

EXTRA_DIST = $(man_MANS)

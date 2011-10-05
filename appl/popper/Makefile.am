# $Id$

include $(top_srcdir)/Makefile.am.common

noinst_PROGRAMS = pop_debug

libexec_PROGRAMS = popper

popper_SOURCES =				\
	pop_auth.c				\
	pop_auth.h				\
	pop_dele.c				\
	pop_dropcopy.c				\
	pop_dropinfo.c				\
	pop_get_command.c			\
	pop_init.c				\
	pop_last.c				\
	pop_list.c				\
	pop_log.c				\
	pop_msg.c				\
	pop_parse.c				\
	pop_pass.c				\
	pop_quit.c				\
	pop_rset.c				\
	pop_send.c				\
	pop_stat.c				\
	pop_uidl.c				\
	pop_updt.c				\
	pop_user.c				\
	pop_xover.c				\
	popper.c				\
	maildir.c				\
	auth_gssapi.c				\
	popper.h				\
	version.h

LDADD = \
	$(LIB_otp) \
	$(top_builddir)/lib/gssapi/libgssapi.la \
	$(LIB_krb5) \
	$(LIB_hcrypto) \
	$(LIB_roken) \
	$(DBLIB)

man_MANS = popper.8

EXTRA_DIST = NTMakefile pop3.rfc1081 pop3e.rfc1082 \
	popper.README.release README-FIRST \
	$(man_MANS)

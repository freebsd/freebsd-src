# $Id: Makefile.am,v 1.13 2000/11/15 22:51:09 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

noinst_PROGRAMS = pop_debug

libexec_PROGRAMS = popper

popper_SOURCES = \
	pop_dele.c pop_dropcopy.c pop_dropinfo.c \
	pop_get_command.c pop_init.c \
	pop_last.c pop_list.c pop_log.c \
	pop_msg.c pop_parse.c pop_pass.c pop_quit.c \
	pop_rset.c pop_send.c pop_stat.c pop_updt.c \
	pop_user.c pop_uidl.c pop_xover.c popper.c \
	maildir.c popper.h version.h

EXTRA_DIST = pop3.rfc1081 pop3e.rfc1082 \
	popper.README.release README-FIRST README-KRB4

LDADD = \
	$(LIB_otp) \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_des) \
	$(LIB_roken) \
	$(DBLIB)

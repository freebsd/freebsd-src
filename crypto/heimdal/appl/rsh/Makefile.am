# $Id: Makefile.am,v 1.17 2001/07/31 09:12:03 joda Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) -I$(srcdir)/../login

bin_PROGRAMS = rsh

man_MANS = rsh.1 rshd.8

libexec_PROGRAMS = rshd

rsh_SOURCES  = rsh.c common.c rsh_locl.h

rshd_SOURCES = rshd.c common.c login_access.c rsh_locl.h

login_access.c:
	$(LN_S) $(srcdir)/../login/login_access.c .

LDADD = $(LIB_kafs) \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_des) \
	$(LIB_roken) \
	$(LIB_kdfs)

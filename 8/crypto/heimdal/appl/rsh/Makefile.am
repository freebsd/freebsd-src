# $Id: Makefile.am 20466 2007-04-20 08:29:05Z lha $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4) -I$(srcdir)/../login

bin_PROGRAMS = rsh

man_MANS = rsh.1 rshd.8

libexec_PROGRAMS = rshd

rsh_SOURCES  = rsh.c common.c rsh_locl.h

rshd_SOURCES = rshd.c common.c login_access.c limits_conf.c rsh_locl.h

login_access.c:
	$(LN_S) $(srcdir)/../login/login_access.c .

limits_conf.c:
	$(LN_S) $(srcdir)/../login/limits_conf.c .

LDADD = $(LIB_kafs) \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_hcrypto) \
	$(LIB_roken)

EXTRA_DIST = $(man_MANS)

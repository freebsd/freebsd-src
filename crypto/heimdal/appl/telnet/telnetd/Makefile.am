# $Id: Makefile.am,v 1.14 2000/11/15 22:51:11 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I$(srcdir)/.. $(INCLUDE_krb4)

libexec_PROGRAMS = telnetd

CHECK_LOCAL = 

telnetd_SOURCES  = telnetd.c state.c termstat.c slc.c sys_term.c \
		   utility.c global.c authenc.c defs.h ext.h telnetd.h

man_MANS = telnetd.8

LDADD = \
	../libtelnet/libtelnet.a \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_des) \
	$(LIB_tgetent) \
	$(LIB_logwtmp) \
	$(LIB_roken)

# $Id: Makefile.am,v 1.12 1999/04/09 18:24:38 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I$(srcdir)/.. $(INCLUDE_krb4)

libexec_PROGRAMS = telnetd

CHECK_LOCAL = 

telnetd_SOURCES  = telnetd.c state.c termstat.c slc.c sys_term.c \
		   utility.c global.c authenc.c defs.h ext.h telnetd.h

LDADD = \
	../libtelnet/libtelnet.a \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_tgetent) \
	$(LIB_logwtmp) \
	$(LIB_roken)

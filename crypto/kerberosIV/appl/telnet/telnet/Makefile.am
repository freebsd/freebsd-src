# $Id: Makefile.am,v 1.12 1999/06/23 12:37:58 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I$(srcdir)/.. $(INCLUDE_krb4)

bin_PROGRAMS = telnet

CHECK_LOCAL = 

telnet_SOURCES  = authenc.c commands.c main.c network.c ring.c \
		  sys_bsd.c telnet.c terminal.c \
		  utilities.c defines.h externs.h ring.h telnet_locl.h types.h

LDADD = ../libtelnet/libtelnet.a \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_tgetent) \
	$(LIB_roken)

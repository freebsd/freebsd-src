# $Id: Makefile.am,v 1.16 2001/08/28 11:21:16 joda Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I$(srcdir)/.. $(INCLUDE_krb4) $(INCLUDE_des)

bin_PROGRAMS = telnet

CHECK_LOCAL = 

telnet_SOURCES  = authenc.c commands.c main.c network.c ring.c \
		  sys_bsd.c telnet.c terminal.c \
		  utilities.c defines.h externs.h ring.h telnet_locl.h types.h

man_MANS = telnet.1

LDADD = ../libtelnet/libtelnet.a \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(LIB_des) \
	$(LIB_tgetent) \
	$(LIB_kdfs) \
	$(LIB_roken)

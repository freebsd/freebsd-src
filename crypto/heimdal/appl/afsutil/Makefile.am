# $Id: Makefile.am,v 1.11 1999/06/27 00:45:26 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

if KRB4
AFSPROGS = afslog pagsh
endif
bin_PROGRAMS = $(AFSPROGS)

afslog_SOURCES = afslog.c

pagsh_SOURCES  = pagsh.c

LDADD = $(LIB_kafs) \
	$(LIB_krb4) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_roken)

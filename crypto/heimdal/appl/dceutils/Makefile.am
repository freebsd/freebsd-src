# $Id: Makefile.am,v 1.8 2002/08/12 15:03:43 joda Exp $

include $(top_srcdir)/Makefile.am.common


DFSPROGS = k5dcecon
if AIX
AIX_DFSPROGS = dpagaix
endif

libexec_PROGRAMS = $(DFSPROGS) $(AIX_DFSPROGS)

dpagaix_CFLAGS = $(dpagaix_cflags)
dpagaix_LDFLAGS = $(dpagaix_ldflags)
dpagaix_LDADD = $(dpagaix_ldadd)

dpagaix$(EXEEXT): $(dpagaix_OBJECTS)
	ld -edpagaix -o dpagaix$(EXEEXT) $(dpagaix_OBJECTS) $(srcdir)/dfspag.exp

LIB_dce = -ldce

k5dcecon_SOURCES  = k5dcecon.c k5dce.h

dpagaix_SOURCES = dpagaix.c

if IRIX
LDADD = $(LIB_dce)
else
LDADD = $(LIB_roken) $(LIB_dce)
endif

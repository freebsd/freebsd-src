# $Id: Makefile.am,v 1.6 2001/07/15 04:21:07 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

DEFS = @DEFS@

foodir = $(libdir)
foo_DATA = afskauthlib.so

SUFFIXES += .c .o

SRCS = verify.c
OBJS = verify.o

CLEANFILES = $(foo_DATA) $(OBJS) so_locations

afskauthlib.so: $(OBJS)
	$(LINK) -shared $(OBJS) $(L)

.c.o:
	$(COMPILE) -c $<

if KRB4
KAFS = $(top_builddir)/lib/kafs/libkafs.la
endif

if KRB5
L = \
	$(KAFS)	\
	$(top_builddir)/lib/krb5/libkrb5.la	\
	$(top_builddir)/lib/asn1/libasn1.la	\
	$(LIB_krb4)				\
	$(LIB_des)				\
	$(top_builddir)/lib/roken/libroken.la	\
	-lc

else

L = \
	$(KAFS)	\
	$(LIB_krb4)				\
	$(LIB_des)				\
	$(top_builddir)/lib/roken/libroken.la	\
	-lc
endif

$(OBJS): $(top_builddir)/include/config.h

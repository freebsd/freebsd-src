# $Id: Makefile.am,v 1.3 1999/04/08 12:35:33 joda Exp $

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
	$(LD) -shared -o $@ $(LDFLAGS) $(OBJS) $(L)

.c.o:
	$(COMPILE) -c $<

if KRB4
KAFS = $(top_builddir)/lib/kafs/.libs/libkafs.a
endif

L = \
	$(KAFS)	\
	$(top_builddir)/lib/krb5/.libs/libkrb5.a	\
	$(top_builddir)/lib/asn1/.libs/libasn1.a	\
	$(LIB_krb4)					\
	$(top_builddir)/lib/des/.libs/libdes.a		\
	$(top_builddir)/lib/roken/.libs/libroken.a	\
	-lc

$(OBJS): $(top_builddir)/include/config.h

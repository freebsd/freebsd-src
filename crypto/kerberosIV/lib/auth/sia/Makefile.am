# $Id: Makefile.am,v 1.4 1999/04/08 12:36:40 joda Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

WFLAGS += $(WFLAGS_NOIMPLICITINT)

DEFS = @DEFS@

## this is horribly ugly, but automake/libtool doesn't allow us to
## unconditionally build shared libraries, and it does not allow us to
## link with non-installed libraries

if KRB4
KAFS=$(top_builddir)/lib/kafs/.libs/libkafs.a
endif

L = \
	$(KAFS)						\
	$(top_builddir)/lib/krb5/.libs/libkrb5.a	\
	$(top_builddir)/lib/asn1/.libs/libasn1.a	\
	$(LIB_krb4)					\
	$(top_builddir)/lib/des/.libs/libdes.a		\
	$(top_builddir)/lib/com_err/.libs/libcom_err.a	\
	$(top_builddir)/lib/roken/.libs/libroken.a	\
	$(LIB_getpwnam_r)				\
	-lc

EXTRA_DIST = sia.c krb5_matrix.conf krb5+c2_matrix.conf security.patch

foodir = $(libdir)
foo_DATA = libsia_krb5.so

LDFLAGS = -rpath $(libdir) -hidden -exported_symbol siad_\* 

OBJS = sia.o posix_getpw.o

libsia_krb5.so: $(OBJS)
	ld -shared -o $@ $(LDFLAGS) $(OBJS) $(L)
	ostrip -x -z $@

CLEANFILES = libsia_krb5.so $(OBJS) so_locations

SUFFIXES += .c .o

.c.o:
	$(COMPILE) -c $<

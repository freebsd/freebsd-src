# $Id: Makefile.am,v 1.9 2001/03/06 16:57:09 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

WFLAGS += $(WFLAGS_NOIMPLICITINT)

DEFS = @DEFS@

## this is horribly ugly, but automake/libtool doesn't allow us to
## unconditionally build shared libraries, and it does not allow us to
## link with non-installed libraries

if KRB4
KAFS=$(top_builddir)/lib/kafs/.libs/libkafs.a
KAFS_S=$(top_builddir)/lib/kafs/.libs/libkafs.so
endif

if KRB5
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

L_shared = \
	$(KAFS_S)					\
	$(top_builddir)/lib/krb5/.libs/libkrb5.so	\
	$(top_builddir)/lib/asn1/.libs/libasn1.so	\
	$(LIB_krb4)					\
	$(top_builddir)/lib/des/.libs/libdes.so		\
	$(top_builddir)/lib/com_err/.libs/libcom_err.so	\
	$(top_builddir)/lib/roken/.libs/libroken.so	\
	$(LIB_getpwnam_r)				\
	-lc

MOD = libsia_krb5.so

else

L = \
	$(KAFS)						\
	$(top_builddir)/lib/kadm/.libs/libkadm.a	\
	$(top_builddir)/lib/krb/.libs/libkrb.a		\
	$(top_builddir)/lib/des/.libs/libdes.a		\
	$(top_builddir)/lib/com_err/.libs/libcom_err.a	\
	$(top_builddir)/lib/roken/.libs/libroken.a	\
	$(LIB_getpwnam_r)				\
	-lc

L_shared = \
	$(KAFS_S)					\
	$(top_builddir)/lib/kadm/.libs/libkadm.so	\
	$(top_builddir)/lib/krb/.libs/libkrb.so		\
	$(top_builddir)/lib/des/.libs/libdes.so		\
	$(top_builddir)/lib/com_err/.libs/libcom_err.so	\
	$(top_builddir)/lib/roken/.libs/libroken.so	\
	$(LIB_getpwnam_r)				\
	-lc

MOD = libsia_krb4.so

endif

EXTRA_DIST = sia.c krb4_matrix.conf krb4+c2_matrix.conf \
	krb5_matrix.conf krb5+c2_matrix.conf security.patch

foodir = $(libdir)
foo_DATA = $(MOD)

LDFLAGS = -rpath $(libdir) -hidden -exported_symbol siad_\* 

OBJS = sia.o posix_getpw.o

libsia_krb5.so: $(OBJS)
	@if test -f $(top_builddir)/lib/krb5/.libs/libkrb5.a; then \
		echo "ld -shared -o $@ $(LDFLAGS) $(OBJS) $(L)"; \
		ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L) | sed 's,-R,-rpath,g'`; \
	elif test -f $(top_builddir)/lib/krb5/.libs/libkrb5.so; then \
		echo "ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L_shared) | sed 's,-R,-rpath,g'`"; \
		ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L_shared) | sed 's,-R,-rpath,g'`; \
	else \
		echo "missing libraries"; exit 1; \
	fi
	ostrip -x -z $@

libsia_krb4.so: $(OBJS)
	@if test -f $(top_builddir)/lib/krb/.libs/libkrb.a; then \
		echo "ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L) | sed 's,-R,-rpath,g'`"; \
		ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L) | sed 's,-R,-rpath,g'`; \
	elif test -f $(top_builddir)/lib/krb/.libs/libkrb.so; then \
		echo "ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L_shared) | sed 's,-R,-rpath,g'`"; \
		ld -shared -o $@ $(LDFLAGS) $(OBJS) `echo $(L_shared) | sed 's,-R,-rpath,g'`; \
	else \
		echo "missing libraries"; exit 1; \
	fi
	ostrip -x $@

CLEANFILES = $(MOD) $(OBJS) so_locations

SUFFIXES += .c .o

.c.o:
	$(COMPILE) -c $<

# $Id: Makefile.am,v 1.4 2002/05/19 18:43:44 joda Exp $

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

L = \
	$(KAFS)						\
	$(top_builddir)/lib/krb/.libs/libkrb.a		\
	$(LIB_des_a)		\
	$(top_builddir)/lib/roken/.libs/libroken.a	\
	-lc

L_shared = \
	$(KAFS_S)					\
	$(top_builddir)/lib/krb/.libs/libkrb.so		\
	$(LIB_des_so)		\
	$(top_builddir)/lib/roken/.libs/libroken.so	\
	$(LIB_getpwnam_r)				\
	-lc

MOD = pam_krb4.so

endif

EXTRA_DIST = pam.conf.add

foodir = $(libdir)
foo_DATA = $(MOD)

LDFLAGS = @LDFLAGS@

OBJS = pam.o

pam_krb4.so: $(OBJS)
	@if test -f $(top_builddir)/lib/krb/.libs/libkrb.a; then \
		echo "$(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L)"; \
		$(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L); \
	elif test -f $(top_builddir)/lib/krb/.libs/libkrb.so; then \
		echo "$(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L_shared)"; \
		$(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L_shared); \
	else \
		echo "missing libraries"; exit 1; \
	fi

CLEANFILES = $(MOD) $(OBJS)

SUFFIXES += .c .o

.c.o:
	$(COMPILE) -c $<

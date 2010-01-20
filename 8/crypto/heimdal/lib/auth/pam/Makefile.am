# $Id: Makefile.am 22299 2007-12-14 06:39:19Z lha $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4)

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
	$(LIB_hcrypto_a)		\
	$(top_builddir)/lib/roken/.libs/libroken.a	\
	-lc

L_shared = \
	$(KAFS_S)					\
	$(top_builddir)/lib/krb/.libs/libkrb.so		\
	$(LIB_hcrypto_so)		\
	$(top_builddir)/lib/roken/.libs/libroken.so	\
	$(LIB_getpwnam_r)				\
	-lc

MOD = pam_krb4.so

endif

foodir = $(libdir)
foo_DATA = $(MOD)

LDFLAGS = @LDFLAGS@

SRCS = pam.c
OBJS = pam.o

pam_krb4.so: $(OBJS)
	@if test -f $(top_builddir)/lib/krb/.libs/libkrb.a; then \
		echo "$(LIBTOOL) --mode=link --tag=CC $(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L)"; \
		$(LIBTOOL) --mode=link --tag=CC $(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L); \
	elif test -f $(top_builddir)/lib/krb/.libs/libkrb.so; then \
		echo "$(LIBTOOL) --mode=link --tag=CC $(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L_shared)"; \
		$(LIBTOOL) --mode=link --tag=CC $(CC) -shared -o $@ $(LDFLAGS) $(OBJS) $(L_shared); \
	else \
		echo "missing libraries"; exit 1; \
	fi

CLEANFILES = $(MOD) $(OBJS)

SUFFIXES += .c .o

# XXX inline COMPILE since automake wont add it

.c.o:
	$(LIBTOOL) --mode=compile --tag=CC $(CC) \
	$(DEFS) $(DEFAULT_AM_CPPFLAGS) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) \
	-c `test -f '$<' || echo '$(srcdir)/'`$<

EXTRA_DIST = pam.conf.add $(SRCS)

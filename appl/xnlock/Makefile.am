# $Id$

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(X_CFLAGS)

WFLAGS += $(WFLAGS_NOIMPLICITINT)

if HAVE_X

bin_PROGRAMS = xnlock

else

bin_PROGRAMS =

endif

CHECK_LOCAL = no-check-local

man_MANS = xnlock.1

EXTRA_DIST = $(man_MANS) nose.0.left nose.0.right nose.1.left nose.1.right \
	nose.down nose.front nose.left.front nose.right.front \
	NTMakefile

LDADD = \
	$(LIB_kafs) \
	$(LIB_krb5) \
	$(LIB_hcrypto) \
	$(LIB_roken) \
	$(X_LIBS) -lXt $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

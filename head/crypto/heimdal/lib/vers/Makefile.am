# $Id: Makefile.am 21959 2007-10-16 13:25:59Z lha $

include $(top_srcdir)/Makefile.am.common

CLEANFILES		= print_version.h

noinst_LTLIBRARIES	= libvers.la

build_HEADERZ		= vers.h

CHECK_LOCAL		= no-check-local

noinst_PROGRAMS		= make-print-version

if KRB4
if KRB5
## need to link with des here; otherwise, if krb4 is shared the link
## will fail with unresolved references
make_print_version_LDADD = $(LIB_krb4) $(LIB_hcrypto)
endif
endif

libvers_la_SOURCES	= print_version.c

print_version.lo: print_version.h

print_version.h: make-print-version$(EXEEXT)
	./make-print-version$(EXEEXT) print_version.h

make-print-version.o: $(top_builddir)/include/version.h

EXTRA_DIST = $(build_HEADERZ)

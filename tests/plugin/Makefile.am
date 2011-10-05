# $Id$

include $(top_srcdir)/Makefile.am.common

noinst_DATA = krb5.conf

SCRIPT_TESTS = check-pac
TESTS = $(SCRIPT_TESTS)

port = 49188

do_subst = sed -e 's,[@]srcdir[@],$(srcdir),g' \
	-e 's,[@]port[@],$(port),g' \
	-e 's,[@]objdir[@],$(top_builddir)/tests/plugin,g' \
	-e 's,[@]EGREP[@],$(EGREP),g' 

LDADD = ../../lib/krb5/libkrb5.la $(LIB_roken)

check-pac: check-pac.in Makefile
	$(do_subst) < $(srcdir)/check-pac.in > check-pac.tmp
	chmod +x check-pac.tmp
	mv check-pac.tmp check-pac

krb5.conf: krb5.conf.in Makefile
	$(do_subst) < $(srcdir)/krb5.conf.in > krb5.conf.tmp
	mv krb5.conf.tmp krb5.conf

lib_LTLIBRARIES = windc.la

windc_la_SOURCES = windc.c
windc_la_LDFLAGS = -module

CLEANFILES= \
	$(TESTS) \
	server.keytab \
	current-db* \
	foopassword \
	krb5.conf krb5.conf.tmp \
	messages.log

EXTRA_DIST = \
	NTMakefile \
	check-pac.in \
	krb5.conf.in

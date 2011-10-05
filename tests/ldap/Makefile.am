# $Id$

include $(top_srcdir)/Makefile.am.common

noinst_DATA = krb5.conf

check_SCRIPTS = $(TESTS) slapd-init

TESTS = check-ldap

port = 49188

do_subst = sed -e 's,[@]srcdir[@],$(srcdir),g' \
	-e 's,[@]port[@],$(port),g' \
	-e 's,[@]objdir[@],$(top_builddir)/tests/ldap,g' \
	-e 's,[@]EGREP[@],$(EGREP),g' 

check-ldap: check-ldap.in Makefile
	$(do_subst) < $(srcdir)/check-ldap.in > check-ldap.tmp
	chmod +x check-ldap.tmp
	mv check-ldap.tmp check-ldap

slapd-init: slapd-init.in Makefile
	$(do_subst) < $(srcdir)/slapd-init.in > slapd-init.tmp
	chmod +x slapd-init.tmp
	mv slapd-init.tmp slapd-init

krb5.conf: krb5.conf.in Makefile
	$(do_subst) < $(srcdir)/krb5.conf.in > krb5.conf.tmp
	mv krb5.conf.tmp krb5.conf

CLEANFILES= \
	$(TESTS) \
	check-ldap.tmp \
	slapd-init.tmp \
	current-db* \
	krb5.conf krb5.conf.tmp \
	modules.conf \
	cache.krb5 \
	slapd-init \
	foopassword \
	messages.log \
	slapd.pid

EXTRA_DIST = \
	NTMakefile \
	samba.schema \
	slapd.conf \
	slapd-stop \
	check-ldap.in \
	init.ldif \
	krb5.conf.in \
	slapd-init.in

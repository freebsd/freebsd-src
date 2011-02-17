# $Id: Makefile.am 22447 2008-01-15 06:05:17Z lha $

include $(top_srcdir)/Makefile.am.common

noinst_DATA = \
	krb5.conf \
	krb5-pkinit.conf \
	krb5-pkinit-win.conf \
	krb5-slave.conf

check_PROGRAMS = ap-req
check_SCRIPTS = $(SCRIPT_TESTS) 

SCRIPT_TESTS = \
	check-digest \
	check-kadmin \
	check-kdc \
	check-keys \
	check-pkinit \
	check-iprop \
	check-referral \
	check-uu

TESTS = $(SCRIPT_TESTS)

port = 49188
admport = 49189

if HAVE_DLOPEN
do_dlopen = -e 's,[@]DLOPEN[@],true,g'
else
do_dlopen = -e 's,[@]DLOPEN[@],false,g'
endif

do_subst = sed $(do_dlopen) \
	-e 's,[@]srcdir[@],$(srcdir),g' \
	-e 's,[@]port[@],$(port),g' \
	-e 's,[@]admport[@],$(admport),g' \
	-e 's,[@]objdir[@],$(top_builddir)/tests/kdc,g' \
	-e 's,[@]EGREP[@],$(EGREP),g' 

LDADD = ../../lib/krb5/libkrb5.la $(LIB_roken)

check-kdc: check-kdc.in Makefile
	$(do_subst) < $(srcdir)/check-kdc.in > check-kdc.tmp
	chmod +x check-kdc.tmp
	mv check-kdc.tmp check-kdc

check-keys: check-keys.in Makefile
	$(do_subst) < $(srcdir)/check-keys.in > check-keys.tmp
	chmod +x check-keys.tmp
	mv check-keys.tmp check-keys

check-kadmin: check-kadmin.in Makefile
	$(do_subst) < $(srcdir)/check-kadmin.in > check-kadmin.tmp
	chmod +x check-kadmin.tmp
	mv check-kadmin.tmp check-kadmin

check-uu: check-uu.in Makefile
	$(do_subst) < $(srcdir)/check-uu.in > check-uu.tmp
	chmod +x check-uu.tmp
	mv check-uu.tmp check-uu

check-pkinit: check-pkinit.in Makefile krb5-pkinit.conf
	$(do_subst) < $(srcdir)/check-pkinit.in > check-pkinit.tmp
	chmod +x check-pkinit.tmp
	mv check-pkinit.tmp check-pkinit

check-iprop: check-iprop.in Makefile krb5.conf krb5-slave.conf
	$(do_subst) < $(srcdir)/check-iprop.in > check-iprop.tmp
	chmod +x check-iprop.tmp
	mv check-iprop.tmp check-iprop

check-digest: check-digest.in Makefile
	$(do_subst) < $(srcdir)/check-digest.in > check-digest.tmp
	chmod +x check-digest.tmp
	mv check-digest.tmp check-digest

check-referral: check-referral.in Makefile
	$(do_subst) < $(srcdir)/check-referral.in > check-referral.tmp
	chmod +x check-referral.tmp
	mv check-referral.tmp check-referral

krb5.conf: krb5.conf.in Makefile
	$(do_subst) \
	   -e 's,[@]kdc[@],,g' < $(srcdir)/krb5.conf.in > krb5.conf.tmp
	mv krb5.conf.tmp krb5.conf

krb5-slave.conf: krb5.conf.in Makefile
	$(do_subst) \
	   -e 's,[@]kdc[@],.slave,g' < $(srcdir)/krb5.conf.in > krb5-slave.conf.tmp
	mv krb5-slave.conf.tmp krb5-slave.conf

krb5-pkinit.conf: krb5-pkinit.conf.in Makefile
	$(do_subst) -e 's,[@]w2k[@],no,g' < $(srcdir)/krb5-pkinit.conf.in > krb5-pkinit.conf.tmp
	mv krb5-pkinit.conf.tmp krb5-pkinit.conf

krb5-pkinit-win.conf: krb5-pkinit.conf.in Makefile
	$(do_subst) -e 's,[@]w2k[@],yes,g' < $(srcdir)/krb5-pkinit.conf.in > krb5-pkinit-win.conf.tmp
	mv krb5-pkinit-win.conf.tmp krb5-pkinit-win.conf

CLEANFILES= \
	$(TESTS) \
	iprop-stats \
	barpassword \
	cache.krb5 \
	cdigest-reply \
	*.tmp \
	client-cache \
	current-db* \
	current*.log \
	iprop.keytab \
	digest-reply \
	foopassword \
	krb5.conf \
	krb5-slave.conf \
	krb5-pkinit.conf \
	krb5-pkinit-win.conf \
	krb5.conf.keys \
	signal \
	messages.log \
	o2cache.krb5 \
	o2digest-reply \
	ocache.krb5 \
	s2digest-reply \
	sdigest-init \
	sdigest-reply \
	server.keytab \
	req-pkinit.der \
	req-pkinit2.der \
	req-kdc.der \
	pkinit.crt \
	pkinit2.crt \
	pkinit3.crt \
	kdc.crt \
	ca.crt \
	uuserver.log \
	tempfile \
	test-rc-file.rc

EXTRA_DIST = \
	check-kadmin.in \
	check-kdc.in \
	check-keys.in \
	check-referral.in \
	check-uu.in \
	check-pkinit.in \
	check-iprop.in \
	check-digest.in \
	heimdal.acl \
	krb5.conf.in \
	krb5.conf.keys.in \
	krb5-pkinit.conf.in \
	iprop-acl \
	wait-kdc.sh \
	pki-mapping \
	ntlm-user-file.txt \
	uuserver.txt \
	donotexists.txt

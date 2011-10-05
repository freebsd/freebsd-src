include $(top_srcdir)/Makefile.am.common

noinst_DATA = \
	krb5.conf \
	krb5-weak.conf \
	krb5-pkinit.conf \
	krb5-pkinit-win.conf \
	krb5-slave.conf

check_SCRIPTS = $(SCRIPT_TESTS) 

SCRIPT_TESTS = \
	check-cc \
	check-delegation \
	check-des \
	check-digest \
	check-kadmin \
	check-kdc \
	check-kdc-weak \
	check-keys \
	check-kpasswdd \
	check-pkinit \
	check-iprop \
	check-referral \
	check-uu

TESTS = $(SCRIPT_TESTS)

port = 49188
admport = 49189
pwport = 49190

if HAVE_DLOPEN
do_dlopen = -e 's,[@]DLOPEN[@],true,g'
else
do_dlopen = -e 's,[@]DLOPEN[@],false,g'
endif

do_subst = sed $(do_dlopen) \
	-e 's,[@]env_setup[@],$(top_builddir)/tests/bin/setup-env,g' \
	-e 's,[@]srcdir[@],$(srcdir),g' \
	-e 's,[@]port[@],$(port),g' \
	-e 's,[@]admport[@],$(admport),g' \
	-e 's,[@]pwport[@],$(pwport),g' \
	-e 's,[@]objdir[@],$(top_builddir)/tests/kdc,g' \
	-e 's,[@]top_builddir[@],$(top_builddir),g' \
	-e 's,[@]EGREP[@],$(EGREP),g' 

LDADD = ../../lib/krb5/libkrb5.la $(LIB_roken)

check-cc: check-cc.in Makefile
	$(do_subst) < $(srcdir)/check-cc.in > check-cc.tmp
	chmod +x check-cc.tmp
	mv check-cc.tmp check-cc

check-delegation: check-delegation.in Makefile
	$(do_subst) < $(srcdir)/check-delegation.in > check-delegation.tmp
	chmod +x check-delegation.tmp
	mv check-delegation.tmp check-delegation

check-des: check-des.in Makefile krb5.conf
	$(do_subst) < $(srcdir)/check-des.in > check-des.tmp
	chmod +x check-des.tmp
	mv check-des.tmp check-des

check-kdc: check-kdc.in Makefile
	$(do_subst) < $(srcdir)/check-kdc.in > check-kdc.tmp
	chmod +x check-kdc.tmp
	mv check-kdc.tmp check-kdc

check-kdc-weak: check-kdc-weak.in Makefile
	$(do_subst) < $(srcdir)/check-kdc-weak.in > check-kdc-weak.tmp
	chmod +x check-kdc-weak.tmp
	mv check-kdc-weak.tmp check-kdc-weak

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

check-kpasswdd: check-kpasswdd.in Makefile
	$(do_subst) < $(srcdir)/check-kpasswdd.in > check-kpasswdd.tmp
	chmod +x check-kpasswdd.tmp
	mv check-kpasswdd.tmp check-kpasswdd

krb5.conf: krb5.conf.in Makefile
	$(do_subst) \
	   -e 's,[@]WEAK[@],false,g' \
	   -e 's,[@]dk[@],,g' \
	   -e 's,[@]kdc[@],,g' < $(srcdir)/krb5.conf.in > krb5.conf.tmp
	mv krb5.conf.tmp krb5.conf

krb5-weak.conf: krb5.conf.in Makefile
	$(do_subst) \
	   -e 's,[@]WEAK[@],true,g' \
	   -e 's,[@]dk[@],default_keys = aes256-cts-hmac-sha1-96:pw-salt arcfour-hmac-md5:pw-salt des3-cbc-sha1:pw-salt des:pw-salt,g' \
	   -e 's,[@]kdc[@],,g' < $(srcdir)/krb5.conf.in > krb5-weak.conf.tmp
	mv krb5-weak.conf.tmp krb5-weak.conf

krb5-slave.conf: krb5.conf.in Makefile
	$(do_subst) \
	   -e 's,[@]WEAK[@],true,g' \
	   -e 's,[@]dk[@],,g' \
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
	krb5-weak.conf \
	krb5.conf.keys \
	krb5-cc.conf \
	krb5-slave.conf \
	krb5-pkinit.conf \
	krb5-pkinit-win.conf \
	signal \
	leaks-log \
	malloc-log \
	malloc-log-master \
	malloc-log-slave \
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
	pkinit4.crt \
	kdc.crt \
	ca.crt \
	uuserver.log \
	tempfile \
	test-rc-file.rc

EXTRA_DIST = \
	NTMakefile \
	check-cc.in \
	check-delegation.in \
	check-des.in \
	check-digest.in \
	check-iprop.in \
	check-kadmin.in \
	check-kdc.in \
	check-kdc-weak.in \
	check-keys.in \
	check-kpasswdd.in \
	check-pkinit.in \
	check-referral.in \
	check-uu.in \
	donotexists.txt \
	heimdal.acl \
	iprop-acl \
	krb5-pkinit.conf.in \
	krb5.conf.in \
	krb5.conf.keys.in \
	ntlm-user-file.txt \
	leaks-kill.sh \
	pki-mapping \
	uuserver.txt \
	wait-kdc.sh

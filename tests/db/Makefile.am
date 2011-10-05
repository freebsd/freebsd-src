# $Id$

include $(top_srcdir)/Makefile.am.common

noinst_DATA = krb5.conf krb5.conf-sqlite

noinst_SCRIPTS = have-db

check_SCRIPTS = loaddump-db add-modify-delete check-dbinfo check-aliases

TESTS = $(check_SCRIPTS) 

do_subst = sed -e 's,[@]srcdir[@],$(srcdir),g' \
	-e 's,[@]top_builddir[@],$(top_builddir),g' \
	-e 's,[@]objdir[@],$(top_builddir)/tests/db,g' \
	-e 's,[@]EGREP[@],$(EGREP),g' 

loaddump-db: loaddump-db.in Makefile
	$(do_subst) < $(srcdir)/loaddump-db.in > loaddump-db.tmp
	chmod +x loaddump-db.tmp
	mv loaddump-db.tmp loaddump-db

add-modify-delete: add-modify-delete.in Makefile
	$(do_subst) < $(srcdir)/add-modify-delete.in > add-modify-delete.tmp
	chmod +x add-modify-delete.tmp
	mv add-modify-delete.tmp add-modify-delete

check-dbinfo: check-dbinfo.in Makefile
	$(do_subst) < $(srcdir)/check-dbinfo.in > check-dbinfo.tmp
	chmod +x check-dbinfo.tmp
	mv check-dbinfo.tmp check-dbinfo

check-aliases: check-aliases.in Makefile
	$(do_subst) < $(srcdir)/check-aliases.in > check-aliases.tmp
	chmod +x check-aliases.tmp
	mv check-aliases.tmp check-aliases

have-db: have-db.in Makefile
	$(do_subst) < $(srcdir)/have-db.in > have-db.tmp
	chmod +x have-db.tmp
	mv have-db.tmp have-db

krb5.conf: krb5.conf.in Makefile
	$(do_subst) -e 's,[@]type[@],,g' < $(srcdir)/krb5.conf.in > krb5.conf.tmp
	mv krb5.conf.tmp krb5.conf

krb5.conf-sqlite: krb5.conf.in Makefile
	$(do_subst) -e 's,[@]type[@],sqlite:,g' < $(srcdir)/krb5.conf.in > krb5.conf-sqlite.tmp
	mv krb5.conf-sqlite.tmp krb5.conf-sqlite

krb5-mit.conf: krb5-mit.conf.in Makefile
	$(do_subst) < $(srcdir)/krb5-mit.conf.in > krb5-mit.conf.tmp
	mv krb5-mit.conf.tmp krb5-mit.conf


CLEANFILES= \
	$(TESTS) \
	have-db \
	db-dump* \
	dbinfo.out \
	current-db* \
	out-text-dump* \
	out-current-* \
	mkey.file* \
	krb5.conf krb5.conf.tmp \
	krb5.conf-sqlite krb5.conf-sqlite.tmp \
	krb5-mit.conf krb5-mit.conf.tmp \
	tempfile \
	log.current-db* \
	heimdal-db* \
	messages.log

EXTRA_DIST = \
	NTMakefile \
	check-aliases.in \
	check-dbinfo.in \
	loaddump-db.in \
	add-modify-delete.in \
	have-db.in \
	krb5.conf.in \
	krb5-mit.conf.in \
	text-dump-0.7 \
	text-dump-known-ext \
	text-dump-no-ext \
	text-dump-unknown-ext


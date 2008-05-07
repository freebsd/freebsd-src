# $Id: Makefile.am 20599 2007-05-08 02:48:22Z lha $

include $(top_srcdir)/Makefile.am.common

noinst_DATA = krb5.conf

noinst_SCRIPTS = have-db

check_SCRIPTS = loaddump-db add-modify-delete check-dbinfo

TESTS = $(check_SCRIPTS) 

do_subst = sed -e 's,[@]srcdir[@],$(srcdir),g' \
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

have-db: have-db.in Makefile
	$(do_subst) < $(srcdir)/have-db.in > have-db.tmp
	chmod +x have-db.tmp
	mv have-db.tmp have-db

krb5.conf: krb5.conf.in Makefile
	$(do_subst) < $(srcdir)/krb5.conf.in > krb5.conf.tmp
	mv krb5.conf.tmp krb5.conf


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
	tempfile \
	log.current-db* \
	messages.log

EXTRA_DIST = \
	check-dbinfo.in \
	loaddump-db.in \
	add-modify-delete.in \
	have-db.in \
	krb5.conf.in \
	text-dump-0.7 \
	text-dump-known-ext \
	text-dump-no-ext \
	text-dump-unknown-ext


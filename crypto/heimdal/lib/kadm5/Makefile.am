# $Id: Makefile.am,v 1.51 2002/08/16 20:57:09 joda Exp $

include $(top_srcdir)/Makefile.am.common

lib_LTLIBRARIES = libkadm5srv.la libkadm5clnt.la
libkadm5srv_la_LDFLAGS = -version-info 7:5:0
libkadm5clnt_la_LDFLAGS = -version-info 6:3:2
sbin_PROGRAMS = dump_log replay_log truncate_log

libkadm5srv_la_LIBADD = ../krb5/libkrb5.la ../hdb/libhdb.la ../roken/libroken.la
libkadm5clnt_la_LIBADD = ../krb5/libkrb5.la ../hdb/libhdb.la ../roken/libroken.la

libexec_PROGRAMS = ipropd-master ipropd-slave

kadm5includedir = $(includedir)/kadm5
buildkadm5include = $(buildinclude)/kadm5

kadm5include_HEADERS = kadm5_err.h admin.h private.h \
	kadm5-protos.h kadm5-private.h

install-build-headers:: $(kadm5include_HEADERS)
	@foo='$(kadm5include_HEADERS)'; \
	for f in $$foo; do \
		f=`basename $$f`; \
		if test -f "$(srcdir)/$$f"; then file="$(srcdir)/$$f"; \
		else file="$$f"; fi; \
		if cmp -s  $$file $(buildkadm5include)/$$f 2> /dev/null ; then \
		: ; else \
			echo "cp $$file $(buildkadm5include)/$$f";\
			cp $$file $(buildkadm5include)/$$f; \
		fi ; \
	done

SOURCES_client =				\
	admin.h					\
	chpass_c.c				\
	common_glue.c				\
	create_c.c				\
	delete_c.c				\
	destroy_c.c				\
	flush_c.c				\
	free.c					\
	get_c.c					\
	get_princs_c.c				\
	init_c.c				\
	kadm5_err.c				\
	kadm5_locl.h				\
	marshall.c				\
	modify_c.c				\
	private.h				\
	privs_c.c				\
	randkey_c.c				\
	rename_c.c				\
	send_recv.c

SOURCES_server =					\
	acl.c					\
	admin.h					\
	bump_pw_expire.c			\
	chpass_s.c				\
	common_glue.c				\
	context_s.c				\
	create_s.c				\
	delete_s.c				\
	destroy_s.c				\
	ent_setup.c				\
	error.c					\
	flush_s.c				\
	free.c					\
	get_princs_s.c				\
	get_s.c					\
	init_s.c				\
	kadm5_err.c				\
	kadm5_locl.h				\
	keys.c					\
	log.c					\
	marshall.c				\
	modify_s.c				\
	private.h				\
	privs_s.c				\
	randkey_s.c				\
	rename_s.c				\
	set_keys.c				\
	set_modifier.c				\
	password_quality.c

libkadm5srv_la_SOURCES = $(SOURCES_server) server_glue.c
libkadm5clnt_la_SOURCES = $(SOURCES_client) client_glue.c

dump_log_SOURCES = dump_log.c kadm5_locl.h

replay_log_SOURCES = replay_log.c kadm5_locl.h

ipropd_master_SOURCES = ipropd_master.c iprop.h kadm5_locl.h

ipropd_slave_SOURCES = ipropd_slave.c iprop.h kadm5_locl.h

truncate_log_SOURCES = truncate_log.c

LDADD = \
	libkadm5srv.la \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(LIB_openldap) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_des) \
	$(LIB_roken) \
	$(DBLIB) \
	$(LIB_dlopen) \
	$(LIB_pidfile)

CLEANFILES = kadm5_err.c kadm5_err.h

$(libkadm5srv_la_OBJECTS): kadm5_err.h

client_glue.lo server_glue.lo: $(srcdir)/common_glue.c

# to help stupid solaris make

kadm5_err.h: kadm5_err.et

$(libkadm5clnt_la_OBJECTS) $(libkadm5srv_la_OBJECTS): $(srcdir)/kadm5-protos.h $(srcdir)/kadm5-private.h

proto_opts = -q -R '^(_|kadm5_c_|kadm5_s_|kadm5_log)' -P comment
$(srcdir)/kadm5-protos.h:
	cd $(srcdir); perl ../../cf/make-proto.pl $(proto_opts) \
		-o kadm5-protos.h \
		$(libkadm5clnt_la_SOURCES) $(libkadm5srv_la_SOURCES) \
		|| rm -f kadm5-protos.h

$(srcdir)/kadm5-private.h:
	cd $(srcdir); perl ../../cf/make-proto.pl $(proto_opts) \
		-p kadm5-private.h \
		$(libkadm5clnt_la_SOURCES) $(libkadm5srv_la_SOURCES) \
		|| rm -f kadm5-private.h

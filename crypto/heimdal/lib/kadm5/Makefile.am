# $Id: Makefile.am,v 1.33 2000/02/07 03:37:27 assar Exp $

include $(top_srcdir)/Makefile.am.common

lib_LTLIBRARIES = libkadm5srv.la libkadm5clnt.la
libkadm5srv_la_LDFLAGS = -version-info 5:1:0
libkadm5clnt_la_LDFLAGS = -version-info 4:2:0
sbin_PROGRAMS = dump_log replay_log

libexec_PROGRAMS = ipropd-master ipropd-slave

kadm5includedir = $(includedir)/kadm5
buildkadm5include = $(buildinclude)/kadm5

kadm5include_HEADERS = kadm5_err.h admin.h private.h

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

C_SOURCES =		\
	admin.h		\
	chpass_c.c	\
	common_glue.c	\
	create_c.c	\
	delete_c.c	\
	destroy_c.c	\
	flush_c.c	\
	free.c		\
	get_c.c		\
	get_princs_c.c	\
	init_c.c	\
	kadm5_err.c	\
	kadm5_locl.h	\
	marshall.c	\
	modify_c.c	\
	private.h	\
	privs_c.c	\
	randkey_c.c	\
	rename_c.c	\
	send_recv.c

S_SOURCES =		\
	acl.c		\
	admin.h		\
	chpass_s.c	\
	common_glue.c	\
	context_s.c	\
	create_s.c	\
	delete_s.c	\
	destroy_s.c	\
	ent_setup.c	\
	error.c		\
	flush_s.c	\
	free.c		\
	get_princs_s.c	\
	get_s.c		\
	init_s.c	\
	kadm5_err.c	\
	kadm5_locl.h	\
	log.c		\
	marshall.c	\
	modify_s.c	\
	private.h	\
	privs_s.c	\
	randkey_s.c	\
	rename_s.c	\
	set_keys.c	\
	set_modifier.c	\
	password_quality.c

libkadm5srv_la_SOURCES = $(S_SOURCES) server_glue.c
libkadm5clnt_la_SOURCES = $(C_SOURCES) client_glue.c

dump_log_SOURCES = dump_log.c kadm5_locl.h

replay_log_SOURCES = replay_log.c kadm5_locl.h

ipropd_master_SOURCES = ipropd_master.c iprop.h kadm5_locl.h

ipropd_slave_SOURCES = ipropd_slave.c iprop.h kadm5_locl.h

LDADD = \
	libkadm5srv.la \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_roken) \
	$(DBLIB) \
	$(LIB_dlopen)

CLEANFILES = kadm5_err.c kadm5_err.h

$(libkadm5srv_la_OBJECTS): kadm5_err.h

client_glue.lo server_glue.lo: $(srcdir)/common_glue.c

# to help stupid solaris make

kadm5_err.h: kadm5_err.et

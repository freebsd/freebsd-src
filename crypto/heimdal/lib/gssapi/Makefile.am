# $Id: Makefile.am,v 1.19 2000/02/07 04:00:51 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I$(srcdir)/../krb5

lib_LTLIBRARIES = libgssapi.la
libgssapi_la_LDFLAGS = -version-info 0:5:0

include_HEADERS = gssapi.h

libgssapi_la_SOURCES =		\
	8003.c			\
	accept_sec_context.c	\
	acquire_cred.c		\
	add_oid_set_member.c	\
	canonicalize_name.c	\
	compare_name.c		\
	context_time.c		\
	create_emtpy_oid_set.c	\
	decapsulate.c		\
	delete_sec_context.c	\
	display_name.c		\
	display_status.c	\
	duplicate_name.c	\
	encapsulate.c		\
	export_name.c		\
	external.c		\
	get_mic.c		\
	gssapi.h		\
	gssapi_locl.h		\
	import_name.c		\
	indicate_mechs.c	\
	init.c			\
	init_sec_context.c	\
	inquire_context.c	\
	inquire_cred.c		\
	release_buffer.c	\
	release_cred.c		\
	release_name.c		\
	release_oid_set.c	\
	test_oid_set_member.c	\
	unwrap.c		\
	v1.c			\
	verify_mic.c		\
	wrap.c

# $FreeBSD$

LIB=	gssapi_ntlm
LDFLAGS= -Wl,-Bsymbolic
LDADD=	-lkrb5 -lhx509 -lheimntlm -lroken
DPADD=	${LIBKRB5} ${LIBHX509} ${LIBHEIMNTLM} ${LIBROKEN}

SRCS=	accept_sec_context.c \
	acquire_cred.c \
	add_cred.c \
	canonicalize_name.c \
	compare_name.c \
	context_time.c \
	crypto.c \
	delete_sec_context.c \
	display_name.c \
	display_status.c \
	duplicate_name.c \
	export_name.c \
	export_sec_context.c \
	external.c \
	ntlm.h \
	ntlm-private.h \
	import_name.c \
	import_sec_context.c \
	indicate_mechs.c \
	init_sec_context.c \
	inquire_context.c \
	inquire_cred.c \
	inquire_cred_by_mech.c \
	inquire_mechs_for_name.c \
	inquire_names_for_mech.c \
	prefix.c \
	process_context_token.c \
	release_cred.c \
	release_name.c \
	digest.c

CFLAGS+=-I${KRB5DIR}/lib/gssapi
CFLAGS+=-I${KRB5DIR}/lib/ntlm

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/gssapi/ntlm ${.CURDIR}/../../../lib/libgssapi

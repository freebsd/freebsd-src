# $FreeBSD$

LIB=	gssapi_krb5
LDFLAGS= -Wl,-Bsymbolic
LDADD=	-lkrb5 -lcrypto -lroken -lasn1 -lcom_err -lcrypt
DPADD=	${LIBKRB5} ${LIBCRYPTO} ${LIBROKEN} ${LIBASN1} ${LIBCOM_ERR} \
	${LIBCRYPT}
NO_MAN=

SRCS=	8003.c \
	accept_sec_context.c \
	acquire_cred.c \
	add_cred.c \
	add_oid_set_member.c \
	address_to_krb5addr.c \
	arcfour.c \
	canonicalize_name.c \
	compare_name.c \
	compat.c \
	context_time.c \
	copy_ccache.c \
	create_emtpy_oid_set.c \
	decapsulate.c \
	delete_sec_context.c \
	display_name.c \
	display_status.c \
	duplicate_name.c \
	encapsulate.c \
	export_name.c \
	export_sec_context.c \
	external.c \
	get_mic.c \
	import_name.c \
	import_sec_context.c \
	indicate_mechs.c \
	init.c \
	init_sec_context.c \
	inquire_context.c \
	inquire_cred.c \
	inquire_cred_by_mech.c \
	inquire_mechs_for_name.c \
	inquire_names_for_mech.c \
	process_context_token.c \
	release_buffer.c \
	release_cred.c \
	release_name.c \
	release_oid_set.c \
	test_oid_set_member.c \
	unwrap.c \
	v1.c \
	verify_mic.c \
	wrap.c

CFLAGS+=-I${KRB5DIR}/lib/gssapi -I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/gssapi

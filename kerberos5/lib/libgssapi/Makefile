# $FreeBSD$

LIB=	gssapi
CFLAGS+=-I${KRB5DIR}/lib/gssapi	\
	-I${KRB5DIR}/lib/krb5   \
	-I${KRB5DIR}/lib/asn1	\
	-I${KRB5DIR}/lib/roken  \
	-I${KRB5DIR}/lib/des	\
	-I${KRB5DIR}/include	\
	-I${ASN1OBJDIR}		\
	-I${.OBJDIR}

SRCS=				\
	8003.c			\
	accept_sec_context.c	\
	acquire_cred.c		\
	add_oid_set_member.c	\
	canonicalize_name.c	\
	compare_name.c		\
	context_time.c		\
	copy_ccache.c		\
	create_emtpy_oid_set.c	\
	decapsulate.c		\
	delete_sec_context.c	\
	display_name.c		\
	display_status.c	\
	duplicate_name.c	\
	encapsulate.c		\
	export_sec_context.c	\
	export_name.c		\
	external.c		\
	get_mic.c		\
	gssapi.h		\
	gssapi_locl.h		\
	import_name.c		\
	import_sec_context.c	\
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
        wrap.c                  \
        address_to_krb5addr.c

INCS=	gssapi.h heim_err.h krb5_err.h k524_err.h

.include <bsd.lib.mk>

.PATH:		${KRB5DIR}/lib/gssapi

beforedepend all:	heim_err.h krb5_err.h k524_err.h

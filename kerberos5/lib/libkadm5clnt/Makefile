# $FreeBSD$

LIB=	kadm5clnt

INCS=	\
	admin.h			\
	kadm5-private.h		\
	kadm5-protos.h		\
	kadm5_err.h		\
	private.h
INCSDIR=${INCLUDEDIR}/kadm5

SRCS=	\
	chpass_c.c			\
	common_glue.c			\
	create_c.c			\
	delete_c.c			\
	destroy_c.c			\
	flush_c.c			\
	free.c				\
	get_c.c				\
	get_princs_c.c			\
	init_c.c			\
	kadm5_err.c			\
	marshall.c			\
	modify_c.c			\
	privs_c.c			\
	randkey_c.c			\
	rename_c.c			\
	send_recv.c			\
	client_glue.c			\
	admin.h				\
	private.h			\
	kadm5_locl.h			\
	${.OBJDIR}/kadm5_err.h

CFLAGS+=-I${KRB5DIR}/include		\
	-I${KRB5DIR}/lib/krb5		\
	-I${KRB5DIR}/lib/kadm5		\
	-I${KRB5DIR}/lib/hdb		\
	-I${KRB5DIR}/lib/asn1		\
	-I${KRB5DIR}/lib/roken		\
	-I${ASN1OBJDIR}			\
	-I${HDBOBJDIR}			\
	-I${.OBJDIR}

.include <bsd.lib.mk>

.PATH:		${KRB5DIR}/lib/kadm5

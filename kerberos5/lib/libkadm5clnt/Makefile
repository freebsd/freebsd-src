# $FreeBSD$

LIB=	kadm5clnt

INCS=	admin.h \
	kadm5-private.h \
	kadm5-protos.h \
	kadm5_err.h \
	private.h

INCSDIR=${INCLUDEDIR}/kadm5

SRCS=	chpass_c.c \
	client_glue.c \
	common_glue.c \
	create_c.c \
	delete_c.c \
	destroy_c.c \
	flush_c.c \
	free.c \
	get_c.c \
	get_princs_c.c \
	init_c.c \
	kadm5_err.c \
	kadm5_err.h \
	marshall.c \
	modify_c.c \
	privs_c.c \
	randkey_c.c \
	rename_c.c \
	send_recv.c

CFLAGS+=-I${KRB5DIR}/lib/kadm5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/kadm5

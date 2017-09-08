# $FreeBSD$

PROG=	kadmind
MAN=	kadmind.8
SRCS=	rpc.c \
	server.c \
	kadmind.c \
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/base \
	-I${KRB5DIR}/lib/wind \
	-I${KRB5DIR}/lib/hx509 \
	-I${KRB5DIR}/lib/hdb \
	-I${.OBJDIR:H:H}/lib/libkrb5 \
	-I${.OBJDIR:H:H}/lib/libroken \
	-I${.OBJDIR:H:H}/lib/libasn1 \
	-I${.OBJDIR:H:H}/lib/libwind \
	-I${.OBJDIR:H:H}/lib/libhx509 \
	-I${.OBJDIR:H:H}/lib/libhdb \
	${LDAPCFLAGS}
LIBADD=	kadm5srv gssapi hdb krb5 roken vers
DPADD=	${LDAPDPADD}
LDADD=	${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin

# $FreeBSD$

PROG=	kadmind
MAN=	kadmind.8
SRCS=	rpc.c \
	server.c \
	kadmind.c \
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBGSSAPI} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} \
	${LIBASN1} ${LIBVERS} ${LIBCRYPTO} ${LIBCRYPT} ${LDAPDPADD}
LDADD=	-lkadm5srv -lgssapi -lhdb -lkrb5 -lroken \
	-lasn1 ${LIBVERS} -lcrypto -lcrypt ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

NO_PIE=	yes

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin

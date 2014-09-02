# $FreeBSD$

PROG=	kcm
MAN=	kcm.8

SRCS=	acl.c \
	acquire.c \
	cache.c \
	client.c \
	config.c \
	connect.c \
	events.c \
	glue.c \
	log.c \
	main.c \
	protocol.c \
	sessions.c \
	renew.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kcm -I${KRB5DIR}/lib/ipc ${LDAPCFLAGS}
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBHEIMNTLM} \
	${LIBHEIMIPCS} ${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lroken -lasn1 -lheimntlm ${LDHEIMIPCS} \
	-lcrypto -lcrypt ${LIBVERS} ${LDAPLDADD}
USEPRIVATELIB= heimipcs
LDFLAGS=${LDAPLDFLAGS}

NO_PIE=	yes

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kcm

# $FreeBSD: src/usr.sbin/keyserv/Makefile,v 1.12 2002/09/17 01:49:00 peter Exp $

PROG=	keyserv
MAN=	keyserv.8
SRCS=	keyserv.c setkey.c crypt_svc.c crypt_server.c crypt.h

CFLAGS+= -DKEYSERV_RANDOM -DBROKEN_DES -I.

DPADD=	${LIBMP} ${LIBCRYPTO} ${LIBRPCSVC}
LDADD=	-lmp -lcrypto -lrpcsvc

RPCDIR=	${DESTDIR}/usr/include/rpcsvc

CLEANFILES= crypt_svc.c crypt.h

RPCGEN= rpcgen -C

crypt_svc.c: ${RPCDIR}/crypt.x
	${RPCGEN} -m -o ${.TARGET} ${RPCDIR}/crypt.x

crypt.h: ${RPCDIR}/crypt.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/crypt.x

.include <bsd.prog.mk>

#	$Id: Makefile,v 1.2 1996/11/22 03:00:17 wpaul Exp wpaul $

PROG=	keyserv
SRCS=	keyserv.c setkey.c keyserv_uid.c crypt_svc.c crypt_server.c

MAN8=	keyserv.8

CFLAGS+= -DKEYSERV_RANDOM -DBROKEN_DES -I.

LDADD+= -lmp -lrpcsvc

RPCDIR=	${DESTDIR}/usr/include/rpcsvc

CLEANFILES= crypt_svc.c crypt.h

RPCGEN= rpcgen -C

crypt_svc.c: ${RPCDIR}/crypt.x crypt.h
	${RPCGEN} -m -o ${.TARGET} ${RPCDIR}/crypt.x

crypt.h: ${RPCDIR}/crypt.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/crypt.x

.include <bsd.prog.mk>

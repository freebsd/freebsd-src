# $FreeBSD: src/usr.sbin/keyserv/Makefile,v 1.5 1999/08/28 01:16:41 peter Exp $

PROG=	keyserv
SRCS=	keyserv.c setkey.c keyserv_uid.c crypt_svc.c crypt_server.c crypt.h

MAN8=	keyserv.8

CFLAGS+= -DKEYSERV_RANDOM -DBROKEN_DES -I.
.if $(OBJFORMAT) == elf
CFLAGS+= -DOBJFORMAT_ELF
.endif

DPADD=	${LIBMP} ${LIBRPCSVC}
LDADD=	-lmp -lrpcsvc

RPCDIR=	${DESTDIR}/usr/include/rpcsvc

CLEANFILES= crypt_svc.c crypt.h

RPCGEN= rpcgen -C

crypt_svc.c: ${RPCDIR}/crypt.x
	${RPCGEN} -m -o ${.TARGET} ${RPCDIR}/crypt.x

crypt.h: ${RPCDIR}/crypt.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/crypt.x

.include <bsd.prog.mk>

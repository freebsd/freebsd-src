# $FreeBSD: src/secure/libexec/ssh-keysign/Makefile,v 1.3 2004/01/07 11:17:22 des Exp $

PROG=	ssh-keysign
SRCS=	ssh-keysign.c msg.c readconf.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR}
.if defined(ENABLE_SUID_SSH)
BINMODE=4511
.endif

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

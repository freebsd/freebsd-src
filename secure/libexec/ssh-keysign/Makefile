# $FreeBSD: src/secure/libexec/ssh-keysign/Makefile,v 1.6 2005/06/07 09:31:26 des Exp $

PROG=	ssh-keysign
SRCS=	ssh-keysign.c readconf.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR}
.if defined(ENABLE_SUID_SSH)
BINMODE=4511
.endif

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

# $FreeBSD$

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

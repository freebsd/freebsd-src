# $FreeBSD$

LIBSSH=	${.OBJDIR}/../../lib/libssh/libssh.a

PROG=	ssh-keysign
SRCS=	ssh-keysign.c readconf.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR}
.if defined(ENABLE_SUID_SSH)
BINMODE=4511
.endif

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	${LIBSSH} -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

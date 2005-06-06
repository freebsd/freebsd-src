# $FreeBSD$

LIBSSH=	${.OBJDIR}/../../lib/libssh/libssh.a

PROG=	ssh-keyscan
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	${LIBSSH} -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

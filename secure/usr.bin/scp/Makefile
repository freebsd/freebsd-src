# $FreeBSD$

PROG=	scp
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH}
LDADD=	-lssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

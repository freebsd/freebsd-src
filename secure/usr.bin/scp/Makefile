# $FreeBSD$
#

PROG=	scp
SRCS=	scp.c scp-common.c

DPADD=	${LIBSSH}
LDADD=	-lssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

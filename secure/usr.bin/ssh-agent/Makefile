# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	ssh-agent
BINOWN=	root
BINMODE=555
MAN=	ssh-agent.1

SRCS=	ssh-agent.c log-client.c

.include <bsd.prog.mk>

.PATH:		${SSHSRC}

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypto -lcrypt -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}

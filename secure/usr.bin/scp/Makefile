# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	scp
BINOWN=	root
BINMODE=555
MAN1=	scp.1

SRCS=	scp.c

.include <bsd.prog.mk>

.PATH:		${SSHSRC}

LDADD+=	-lcrypto -lutil -lz -L${.OBJDIR}/../../lib/libssh -lssh
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

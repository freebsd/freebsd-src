# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	ssh-keygen
BINOWN=	root
BINMODE=555
MAN=	ssh-keygen.1

SRCS=	key.c log-client.c ssh-keygen.c

.include <bsd.prog.mk>

.PATH:		${SSHSRC}

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypto -lcrypt -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}

# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	ssh-add
BINOWN=	root
BINMODE=555
MAN1=	ssh-add.1

SRCS=	ssh-add.c log-client.c

.include <bsd.prog.mk>

.PATH:		${SSHSRC}

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypto -lcrypt -lutil -lz
DPADD+= ${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}

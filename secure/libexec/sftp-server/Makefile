# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

.PATH:		${SSHSRC}
#.PATH:		${SSHSRC}/lib

PROG=   sftp-server
BINOWN= root

BINMODE?=555

BINDIR= /usr/libexec
MAN8=    sftp-server.8

SRCS=   sftp-server.c log-server.c

.include <bsd.prog.mk>

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypto
DPADD+=	${LIBCRYPTO}

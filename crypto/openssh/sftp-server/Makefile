.PATH:		${.CURDIR}/..

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

SRCS=	sftp-server.c log-server.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto # -lutil -lz
DPADD+=	${LIBCRYPTO} # ${LIBDES} ${LIBUTIL} ${LIBZ}

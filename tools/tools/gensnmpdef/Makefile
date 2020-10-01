# $FreeBSD$
#
# NB: Install ports/net-mgmt/libsmi

.PATH:	${SRCTOP}/contrib/bsnmp/gensnmpdef

PROG=	gensnmpdef

SRCS=	gensnmpdef.c

MAN=	gensnmpdef.1

LOCALBASE?=	/usr/local

BINDIR=	${LOCALBASE}/bin

MANDIR=	${LOCALBASE}/man/man

CFLAGS+=	-I${LOCALBASE}/include
LDFLAGS+=	-L${LOCALBASE}/lib

LDADD+=	-lsmi

.include <bsd.prog.mk>

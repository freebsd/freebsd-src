# $FreeBSD$

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

DPADD=	${LIBSMB}
LDADD=	-lsmb

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-DSMB_CFG_FILE=\"/etc/nsmb.conf\" -I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>

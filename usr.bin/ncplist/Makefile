#	$FreeBSD$

PROG=	ncplist
SRCS=	ncplist.c
MAN1=	ncplist.1

DPADD+= ${LIBIPX} ${LIBNCP}
LDADD+= -lipx -lncp

.include <bsd.prog.mk>

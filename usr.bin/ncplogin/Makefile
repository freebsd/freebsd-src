#	$FreeBSD$

PROG=	ncplogin
SRCS=	ncplogin.c
MAN1=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD+= -lipx -lncp
DPADD+= ${LIBIPX} ${LIBNCP}

.include <bsd.prog.mk>

PROG=	shar
SRCS=	shar.c tree.c

WARNS?=	6

DPADD=	${LIBARCHIVE}
LDADD=	-larchive

LINKS=	${BINDIR}/shar
MLINKS=	shar.1

.include <bsd.prog.mk>

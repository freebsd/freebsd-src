.include <bsd.own.mk>

PROG=		target
BINDIR=		${TESTSDIR}

WARNS?=		3
CFLAGS+=	-I${.CURDIR}/../libpythagoras

LDFLAGS+=	-L${.OBJDIR}/../libpythagoras
DPADD+=		${.OBJDIR}/../libpythagoras/libpythagoras.a
LDADD=		-lpythagoras

MAN=

.include <bsd.prog.mk>

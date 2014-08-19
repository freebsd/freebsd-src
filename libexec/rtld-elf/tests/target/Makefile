# $FreeBSD$

.include <bsd.own.mk>

PROG=		target
BINDIR=		${TESTSBASE}${TESTSDIR}/libexec/rtld-elf

CFLAGS+=	-I${.CURDIR}/../libpythagoras
LDADD=		-L${.OBJDIR}/../libpythagoras -lpythagoras

MAN=

.include <bsd.prog.mk>

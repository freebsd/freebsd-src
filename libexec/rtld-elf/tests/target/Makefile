# $FreeBSD$

.include <bsd.own.mk>

PROG=		target
BINDIR=		${TESTSBASE}${TESTSDIR}/libexec/rtld-elf

CFLAGS+=	-I${.CURDIR}/../libpythagoras

LDFLAGS+=	-L${.OBJDIR}/../libpythagoras
LDADD=		-lpythagoras

MAN=

.include <bsd.prog.mk>

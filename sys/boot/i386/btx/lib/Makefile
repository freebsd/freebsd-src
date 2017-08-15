# $FreeBSD$

.include <src.opts.mk>
MK_COVERAGE=no

PROG=	crt0.o
INTERNALPROG=
MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>

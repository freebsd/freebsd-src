# $FreeBSD$

PROG=	crt0.o
INTERNALPROG=
MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../../i386/common
LDFLAGS=-Wl,-r

NO_PIE=	yes

.include <bsd.prog.mk>

# $FreeBSD$

.include <bsd.init.mk>

PROG=	crt0.o
INTERNALPROG=
MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${BOOTSRC}/i386/common
LDFLAGS+=-Wl,-r

.include <bsd.prog.mk>

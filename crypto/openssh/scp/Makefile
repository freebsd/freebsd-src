.PATH:		${.CURDIR}/..

PROG=	scp
BINOWN=	root

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "hppa")
BINMODE=0000
.else
BINMODE?=555
.endif

BINDIR=	/usr/bin
MAN=	scp.1

SRCS=	scp.c

.include <bsd.prog.mk>

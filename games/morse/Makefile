#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	morse
MAN=	morse.6

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>

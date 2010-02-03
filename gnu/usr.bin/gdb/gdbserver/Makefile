# $FreeBSD$

# Not elf specific so don't install in /usr/libexec/elf
BINDIR=/usr/bin

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/signals
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb

PROG=	gdbserver

SRCS=	inferiors.c mem-break.c regcache.c remote-utils.c	\
	server.c signals.c target.c utils.c
SRCS+=	fbsd-low.c

.if ${MACHINE_CPUARCH} == "i386"
SRCS+= fbsd-i386-low.c i387-fp.c reg-i386.c
.endif

.if ${MACHINE_CPUARCH} == "arm"
SRCS+= fbsd-arm-low.c reg-arm.c
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
SRCS+= fbsd-ppc-low.c reg-ppc.c
.endif

#CFLAGS+=	-I${.CURDIR}/../arch/${MACHINE_CPUARCH}
CFLAGS+=	-I${GDBDIR}/gdb/gdbserver
CFLAGS+=	-I${GDBDIR}/gdb/regformats
CFLAGS+=	-DNO_MMALLOC -DGDBSERVER

.include <bsd.prog.mk>

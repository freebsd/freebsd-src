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

SRCS+= fbsd-${MACHINE_ARCH}-low.c reg-${MACHINE_ARCH}.c
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+= i387-fp.c
.endif

#CFLAGS+=	-I${.CURDIR}/../arch/${MACHINE_ARCH}
CFLAGS+=	-I${GDBDIR}/gdb/gdbserver
CFLAGS+=	-I${GDBDIR}/gdb/regformats
CFLAGS+=	-DNO_MMALLOC -DGDBSERVER

.include <bsd.prog.mk>

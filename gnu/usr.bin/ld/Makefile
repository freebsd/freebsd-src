# $FreeBSD: src/gnu/usr.bin/ld/Makefile,v 1.30 2000/01/03 10:41:11 obrien Exp $
#

RTLD=	${.CURDIR}/../../../libexec/rtld-aout
GCCDIR=	${.CURDIR}/../../../contrib/gcc

PROG=	ld
BINDIR= /usr/libexec/aout
MAN1aout=ld.1aout
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c support.c rrs.c xbits.c md.c \
	cplus-dem.c
CFLAGS+= -I${.CURDIR} -I${RTLD} -I${RTLD}/${MACHINE_ARCH} \
	-I${GCCDIR} -DIN_GCC -DDEMANGLE_CPLUSPLUS -DFREEBSD_AOUT
NOSHARED?= yes

.PATH: ${RTLD} ${RTLD}/${MACHINE_ARCH} ${GCCDIR}

.include <bsd.prog.mk>

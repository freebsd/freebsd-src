# $FreeBSD$
#

RTLD=	${.CURDIR}/../../../libexec/rtld-aout

PROG=	ld
BINDIR= /usr/libexec/aout
MAN=	ld.1aout
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c support.c rrs.c xbits.c md.c \
	cplus-dem.c
CFLAGS+= -I${.CURDIR} -I${RTLD} -I${RTLD}/${MACHINE_ARCH} \
	-DIN_GCC -DDEMANGLE_CPLUSPLUS -DFREEBSD_AOUT
NOSHARED?= yes

.PATH: ${RTLD} ${RTLD}/${MACHINE_ARCH}

.include <bsd.prog.mk>

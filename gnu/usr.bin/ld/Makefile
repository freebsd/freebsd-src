# $Id: Makefile,v 1.24 1998/06/01 14:08:01 peter Exp $
#

RTLD=	${.CURDIR}/../../../libexec/rtld-aout
GCCDIR=	${.CURDIR}/../../../contrib/gcc

PROG=	ld
BINDIR= /usr/libexec/aout
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c support.c rrs.c xbits.c md.c \
	cplus-dem.c
CFLAGS+= -I${.CURDIR} -I${RTLD} -I${RTLD}/${MACHINE} \
	-I${GCCDIR} -DIN_GCC -DDEMANGLE_CPLUSPLUS -DFREEBSD_AOUT
NOSHARED?= yes

.PATH: ${RTLD} ${RTLD}/${MACHINE} ${GCCDIR}

.include <bsd.prog.mk>

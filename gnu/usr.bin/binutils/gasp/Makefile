# $FreeBSD: src/gnu/usr.bin/binutils/gasp/Makefile,v 1.12 2003/02/25 15:41:48 ru Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=	gasp
SRCS+=	gasp+%FIXED.c macro.c sb.c hash.c
WARNS=	2
CFLAGS+= -DBFD_ASSEMBLER
CFLAGS+= -I${.CURDIR}/../as/${TARGET_ARCH}-freebsd -I${.CURDIR}/../as
CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${SRCDIR} -I${SRCDIR}/gas -I${SRCDIR}/gas/config
DPADD=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}
CLEANFILES= gasp+%FIXED.c

gasp+%FIXED.c: gasp.c
	sed -e 's|xregex\.h|gnuregex.h|g' ${.ALLSRC} >${.TARGET}

.include <bsd.prog.mk>

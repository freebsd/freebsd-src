# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=	gasp
SRCS+=	gasp.c macro.c sb.c hash.c xregex.h
WARNS?=	2
CFLAGS+= -DBFD_ASSEMBLER
CFLAGS+= -I${.CURDIR}/../as/${TARGET_ARCH}-freebsd -I${.CURDIR}/../as
CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${SRCDIR} -I${SRCDIR}/gas -I${SRCDIR}/gas/config
DPADD=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}
CLEANFILES=	xregex.h

xregex.h:
	@echo '#include <regex.h>' >${.TARGET}

.include <bsd.prog.mk>

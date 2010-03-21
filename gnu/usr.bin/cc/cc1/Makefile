# $FreeBSD: src/gnu/usr.bin/cc/cc1/Makefile,v 1.35.12.1 2010/02/10 00:26:20 kensmith Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cc1
SRCS=	main.c c-parser.c c-lang.c
BINDIR=	/usr/libexec
NO_MAN=
NO_SHARED?=yes

OBJS+=  ${PROG}-checksum.o
DPADD=	${LIBBACKEND} ${LIBCPP} ${LIBDECNUMBER} ${LIBIBERTY}
LDADD=	${LIBBACKEND} ${LIBCPP} ${LIBDECNUMBER} ${LIBIBERTY}

DOBJS+=	${SRCS:N*.h:R:S/$/.o/g}
${PROG}-dummy:	${DOBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${DOBJS} ${LDADD}
CLEANFILES+=	${PROG}-dummy

${PROG}-checksum.c:	${PROG}-dummy
	../cc_tools/genchecksum ${PROG}-dummy > ${.TARGET}
CLEANFILES+=	${PROG}-checksum.c

.include <bsd.prog.mk>

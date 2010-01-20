# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/objc ${GCCDIR}

PROG=	cc1obj
SRCS=	main.c c-parser.c objc-act.c objc-lang.c c-decl.c
BINDIR=	/usr/libexec
NO_MAN=
NO_SHARED?=yes

CFLAGS+= -I${GCCDIR}/objc -I.

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

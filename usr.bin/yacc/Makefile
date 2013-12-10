#	@(#)Makefile	5.3 (Berkeley) 5/12/90
# $FreeBSD$

BYACC_SRC=	${.CURDIR}/../../contrib/byacc
.PATH: ${BYACC_SRC}

PROG=	yacc
SRCS=	closure.c error.c graph.c lalr.c lr0.c main.c mkpar.c output.c \
	reader.c skeleton.c symtab.c verbose.c warshall.c

CFLAGS+=	-DMIXEDCASE_FILENAMES=1 \
		-DCC_HAS_PROTOS=1 \
		-DHAVE_FCNTL_H=1 \
		-DHAVE_ATEXIT=1 \
		-DHAVE_MKSTEMP=1

YYPATCH!=	cat ${BYACC_SRC}/VERSION
CFLAGS+=	-DYYPATCH=${YYPATCH}

LINKS=	${BINDIR}/yacc ${BINDIR}/byacc
MLINKS=	yacc.1 byacc.1

.include <bsd.prog.mk>

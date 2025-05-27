.include <src.opts.mk>

BYACC_SRC=	${SRCTOP}/contrib/byacc
.PATH: ${BYACC_SRC}

PACKAGE= toolchain
PROG=	yacc
SRCS=	closure.c error.c graph.c lalr.c lr0.c main.c mkpar.c mstring.c output.c \
	reader.c yaccpar.c symtab.c verbose.c warshall.c

CFLAGS+=	-DHAVE_CONFIG_H -I${.CURDIR}
CFLAGS+=	-DMAXTABLE=INT_MAX

YYPATCH!=	cat ${BYACC_SRC}/VERSION
CFLAGS+=	-DYYPATCH=${YYPATCH}

LINKS=	${BINDIR}/yacc ${BINDIR}/byacc
MLINKS=	yacc.1 byacc.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>

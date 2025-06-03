.include <src.opts.mk>

PACKAGE= toolchain
PROG=	ar
SRCS=	ar.c acplex.l acpyacc.y read.c util.c write.c y.tab.h

LIBADD=	archive elf

NO_WMISSING_VARIABLE_DECLARATIONS=

CFLAGS+=-I. -I${.CURDIR}

LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MLINKS= ar.1 ranlib.1

.include <bsd.prog.mk>

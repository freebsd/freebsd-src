PROG = example
NOMAN = yet

SRCS = example.c

CFLAGS = -static #-g

.if exists(${.CURDIR}/../obj)
FORMDIR=${.CURDIR}/../obj
.else
FORMDIR=${.CURDIR}/..
.endif

LDADD = -L${FORMDIR} -lforms -ldialog -lncurses -lmytinfo -ll
DPADD = ${LIBFORMS} ${LIBNCURSES}

.include <bsd.prog.mk>

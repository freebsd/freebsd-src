PROG = example
NOMAN = yet

SRCS = example.c

CFLAGS = -g -static

.if exists(${.CURDIR}/../obj)
FORMDIR=${.CURDIR}/../obj
.else
FORMDIR=${.CURDIR}/..
.endif

LDADD = -L${FORMDIR} -lforms -lncurses -lmytinfo -ll
DPADD = /usr/lib/libforms.a

.include <bsd.prog.mk>

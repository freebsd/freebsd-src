PROG = test
NOMAN = yet

SRCS = test.c

CFLAGS = 

.if exists(${.CURDIR}/../obj)
FORMDIR=${.CURDIR}/../obj
.else
FORMDIR=${.CURDIR}/..
.endif

LDADD = -L${FORMDIR} -lforms -lncurses -lmytinfo

.include <bsd.prog.mk>

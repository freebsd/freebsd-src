PROG = example
NOMAN = yet

SRCS = example.c frm.tab.h

CFLAGS = 

.if exists(${.CURDIR}/../obj)
FORMDIR=${.CURDIR}/../obj
.else
FORMDIR=${.CURDIR}/..
.endif

LDADD = -L${FORMDIR} -lforms -lncurses -lmytinfo

CLEANFILES += frm.tab.h

frm.tab.h:
	fib example.frm

.include <bsd.prog.mk>

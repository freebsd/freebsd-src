#
# PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
# --------------------         -----   ----------------------
# CURRENT PATCH LEVEL:         1       00092
# --------------------         -----   ----------------------
#
# 15 Mar 93	Patchkit Coordinator	Corrected SRCS, missing .c
#
PROG=expr
SRCS=expr.c
CFLAGS+=-I. -I${.CURDIR}
NOMAN=noman
CLEANFILES+=expr.c y.tab.h

expr.c:
	${YACC} -d ${.IMPSRC}
	mv y.tab.c expr.c

.include <bsd.prog.mk>


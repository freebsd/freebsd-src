PROG = mkmodules
SRCS = mkmodules.c
CFLAGS += -I${.CURDIR}/../cvs -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/obj)
LDADD=  -L${.CURDIR}/../lib/obj -lcvs
.else
LDADD=  -L${.CURDIR}/../lib/ -lcvs
.endif

.include <bsd.prog.mk>
.include "../../Makefile.inc"

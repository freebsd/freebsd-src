PROG = mkmodules
SRCS = mkmodules.c

CFLAGS += -I${.CURDIR}/../cvs -I${.CURDIR}/../lib
LDADD= -L${.CURDIR}/../lib/obj -lcvs

.include <bsd.prog.mk>
.include "../../Makefile.inc"

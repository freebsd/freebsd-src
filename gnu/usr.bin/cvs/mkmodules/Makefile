PROG = mkmodules
SRCS = mkmodules.c
CFLAGS += -I${.CURDIR}/../cvs -I${.CURDIR}/../lib
DPADD+=	${LIBCVS}
LDADD+=	-lcvs

.include "../../Makefile.inc"
.include "../Makefile.inc"
.include <bsd.prog.mk>

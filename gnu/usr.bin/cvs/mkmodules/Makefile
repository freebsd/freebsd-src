#	$Id: Makefile,v 1.6 1995/12/10 23:10:38 peter Exp $

PROG = mkmodules
SRCS = mkmodules.c
CFLAGS+= -I${.CURDIR}/../cvs -I${.CURDIR}/../lib -DHAVE_CONFIG_H
DPADD+=	${LIBCVS}
LDADD+=	-lcvs

.include "../../Makefile.inc"
.include <bsd.prog.mk>

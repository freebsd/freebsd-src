#	$Id: Makefile,v 1.5.6.1 1996/02/23 12:18:51 peter Exp $

PROG = mkmodules
SRCS = mkmodules.c
CFLAGS+= -I${.CURDIR}/../cvs -I${.CURDIR}/../lib -DHAVE_CONFIG_H
DPADD+=	${LIBCVS}
LDADD+=	-lcvs

.include "../../Makefile.inc"
.include <bsd.prog.mk>

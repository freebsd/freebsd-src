#	$Id: Makefile,v 1.5 1995/03/31 07:55:30 nate Exp $

PROG = mkmodules
SRCS = mkmodules.c
CFLAGS+= -I${.CURDIR}/../cvs -I${.CURDIR}/../lib -DHAVE_CONFIG_H
DPADD+=	${LIBCVS}
LDADD+=	-lcvs

.include "../../Makefile.inc"
.include <bsd.prog.mk>

#	$Id: Makefile,v 1.25 1995/03/22 19:39:45 nate Exp $

PROG = mkmodules
SRCS = mkmodules.c
CFLAGS+= -I${.CURDIR}/../cvs -I${.CURDIR}/../lib -DHAVE_CONFIG_H -DHAVE_TIMEZONE
DPADD+=	${LIBCVS}
LDADD+=	-lcvs

.include "../../Makefile.inc"
.include <bsd.prog.mk>

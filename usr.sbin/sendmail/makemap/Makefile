#	@(#)Makefile	8.4 (Berkeley) 6/10/97

PROG=	makemap
MAN8=	makemap.0
CFLAGS+=-I${.CURDIR}/../src -DNEWDB -DNOT_SENDMAIL

SRCS=	makemap.c safefile.c

safefile.c: ${.CURDIR}/../src/safefile.c
	ln -s ${.CURDIR}/../src/safefile.c

.include "../../Makefile.inc"
.include <bsd.prog.mk>

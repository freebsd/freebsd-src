#	$Id: Makefile,v 1.1.2.1 1998/04/05 03:22:30 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/diff

LIB=		diff

NOPROFILE=	yes
NOPIC=		yes

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -I${CVSDIR}/lib -DHAVE_CONFIG_H

SRCS =	diff.c diff3.c analyze.c cmpbuf.c cmpbuf.h io.c context.c ed.c \
        normal.c ifdef.c util.c dir.c version.c diff.h side.c

install:
	@echo -n

.include <bsd.lib.mk>

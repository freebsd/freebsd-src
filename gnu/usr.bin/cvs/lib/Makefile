#	$Id: Makefile,v 1.14 1997/05/15 23:00:07 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man

LIB=		cvs

NOPROFILE=	yes
NOPIC=		yes

CFLAGS+= -I${.CURDIR} -I${CVSDIR}/src -DHAVE_CONFIG_H
YFLAGS=

SRCS =	argmatch.c getdate.y getline.c \
	getopt.c getopt1.c savecwd.c \
	sighandle.c stripslash.c \
	xgetwd.c yesno.c

install:
	@echo -n

.include <bsd.lib.mk>

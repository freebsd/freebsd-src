#	$Id: Makefile,v 1.13 1997/02/22 15:45:20 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man

LIB=		cvs

NOPROFILE=	yes
NOPIC=		yes

CFLAGS+= -I${.CURDIR} -I${CVSDIR}/src -DHAVE_CONFIG_H

SRCS =	argmatch.c getdate.y getline.c \
	getopt.c getopt1.c savecwd.c \
	sighandle.c stripslash.c \
	xgetwd.c yesno.c

CLEANFILES+= getdate.c y.tab.h

install:
	@echo -n

.include <bsd.lib.mk>

#	$Id: Makefile,v 1.10 1995/12/11 02:22:30 peter Exp $

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
	sighandle.c strippath.c stripslash.c \
	xgetwd.c yesno.c

CLEANFILES+= getdate.c y.tab.h

install:
	@echo -n

.include <bsd.lib.mk>

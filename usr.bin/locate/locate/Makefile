#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $Id: Makefile,v 1.1 1996/08/29 22:39:41 wosch Exp wosch $

PROG=	locate
SRCS=	util.c locate.c
CFLAGS+= -I. -DMMAP -O2	# -DDEBUG
MAN1=	locate.1
MAN8=	locate.updatedb.8
SCRIPTS= updatedb mklocatedb concatdb
MLINKS+= locate.updatedb.8 updatedb.8


beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
    	${.CURDIR}/${script}.sh ${DESTDIR}${LIBEXECDIR}/locate.${script}
.endfor

# only /usr/src/etc/Makefile install files in /etc
#	${INSTALL} -c -o root -g wheel -m 644 \
#		${.CURDIR}/locate.rc ${DESTDIR}/etc

.include "../../Makefile.inc"
.include "../Makefile.inc"
.include <bsd.prog.mk>

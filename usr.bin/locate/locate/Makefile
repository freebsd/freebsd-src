#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/locate/locate/Makefile,v 1.11 1999/08/28 01:02:56 peter Exp $

PROG=	locate
SRCS=	util.c locate.c
CFLAGS+= -I${.CURDIR} -DMMAP # -DDEBUG (print time) -O2 (10% faster)
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
.include <bsd.prog.mk>

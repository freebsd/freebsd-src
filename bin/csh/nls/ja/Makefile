# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

NL=     ja
DL=     ja_JP.EUC

.PATH: ${BASESRC}/${NL}

all: tcsh.cat

tcsh.cat: tcsh.msg

tcsh.msg: set[0-9]*
	cat ${BASESRC}/${NL}/set[0-9] \
	    ${BASESRC}/${NL}/set[0-9][0-9] > ${.TARGET}

install:
.for i in ${DL}
	 ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		tcsh.cat ${DESTDIR}${NLSDIR}/$i/tcsh.cat
.endfor

.include <bsd.obj.mk>

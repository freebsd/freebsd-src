# $FreeBSD: src/tools/tools/tinderbox/www/Makefile,v 1.3 2003/08/01 07:55:13 des Exp $

CGI		 = index.cgi
DATA		 = tb.css daemon.png valid-css.png valid-xhtml10.png
WWWDIR		?= ${HOME}/public_html
WWWOWN		?= ${USER}
WWWGRP		?= ${USER}

realinstall:
	${INSTALL} -m0755 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${CGI} ${WWWDIR}
.for FILE in ${DATA}
	${INSTALL} -m0644 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${FILE} ${WWWDIR}
.endfor

.include <bsd.prog.mk>

# $FreeBSD: src/tools/tools/tinderbox/www/Makefile,v 1.2 2003/05/17 14:15:59 des Exp $

CGI		 = index.cgi
DATA		 = tb.css valid-css.gif valid-xhtml10.png
WWWDIR		?= ${HOME}/public_html

realinstall:
	${INSTALL} -m 0755 ${.CURDIR}/${CGI} ${WWWDIR}
.for FILE in ${DATA}
	${INSTALL} -m 0644 ${.CURDIR}/${FILE} ${WWWDIR}
.endfor

.include <bsd.prog.mk>

# $FreeBSD$

CGI		 = index.cgi
DATA		 = tb.css valid-css.gif valid-xhtml10.png
WWWDIR		?= ${HOME}/public_html

realinstall:
	${INSTALL} -m 0755 ${CGI} ${WWWDIR}
	${INSTALL} -m 0644 ${DATA} ${WWWDIR}

.include <bsd.prog.mk>

# $Id: Makefile 2080 2011-10-27 04:23:24Z jkoshy $

PLUGINS=	ardiff teraser

all:	${PLUGINS}

${PLUGINS}:
.for plugin in ${.TARGET}
	${MAKE} -f Makefile.${plugin}
.endfor

clean depend:
.for plugin in ${PLUGINS}
	${MAKE} -f Makefile.${plugin} ${.TARGET}
.endfor

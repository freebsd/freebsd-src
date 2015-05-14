# $Id: Makefile 3192 2015-05-04 17:20:15Z jkoshy $

PLUGINS=	ardiff teraser

all:	${PLUGINS}

${PLUGINS}:
.for plugin in ${.TARGET}
	${MAKE} -f Makefile.${plugin}
.endfor

test:	.PHONY

clean cleandepend depend:	.PHONY
.for plugin in ${PLUGINS}
	${MAKE} -f Makefile.${plugin} ${.TARGET}
.endfor

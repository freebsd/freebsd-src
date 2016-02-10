# $Id: Makefile 3380 2016-01-29 07:53:10Z jkoshy $

PLUGINS=	ardiff teraser

all:	${PLUGINS}

${PLUGINS}:
.for plugin in ${.TARGET}
	${MAKE} -f Makefile.${plugin}
.endfor

test:	.PHONY

clean cleandepend clobber depend:	.PHONY
.for plugin in ${PLUGINS}
	${MAKE} -f Makefile.${plugin} ${.TARGET}
.endfor

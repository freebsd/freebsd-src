FILES=	catman
NOMAN=	noman

CLEANFILES+=	${FILES}

all: ${FILES}

# XXX null suffixes are currently broken
# .SUFFIXES:
# .SUFFIXES: .sh
# .sh:
# 	sed -e 's,%compress%,${compress},' \
# 	    -e 's,%compext%,${compext},' \
# 	    -e 's,%zcat%,${zcat},' \
# 	    ${.IMPSRC} > ${.TARGET}
catman: catman.sh
	sed -e 's,%compress%,${compress},' \
	    -e 's,%compext%,${compext},' \
	    -e 's,%zcat%,${zcat},' \
	    ${.CURDIR}/${.TARGET}.sh > ${.TARGET}

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${FILES} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>

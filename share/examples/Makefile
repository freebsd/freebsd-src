# $FreeBSD$
#
# Doing a make install builds /usr/share/examples

DIRS!=	for i in *; do if test -d $$i -a $$i != CVS; then echo $$i; fi; done

DDIR=	${DESTDIR}/usr/share/examples

NOOBJ=	noobj

all clean cleandir depend lint tags:

beforeinstall: copies
	(cd ${.CURDIR}/../../etc; ${MAKE} etc-examples)

.for dir in ${DIRS}
FILES!=	find -L ${dir} \( -name CVS -prune \) -o -type f -print
.for file in ${FILES}
copies::
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${file} ${DDIR}/${file}
.endfor
.endfor

.include <bsd.prog.mk>

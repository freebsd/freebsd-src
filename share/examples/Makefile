#	$Id: Makefile,v 1.18 1995/12/11 03:03:24 peter Exp $
#
# Doing a make install builds /usr/share/examples

DIRS!=	for i in *; do if test -d $$i -a $$i != CVS; then echo $$i; fi; done

DDIR=	${DESTDIR}/usr/share/examples

NOOBJ=	noobj

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

all clean cleandir depend lint tags:

beforeinstall: ${SHARED}

.for dir in ${DIRS}
FILES!=	find ${dir} \( -name CVS -prune \) -o -type f -print
.for file in ${FILES}
copies::
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 644 ${file} ${DDIR}/${file}
.endfor
.endfor

.for dir in ${DIRS}
symlinks::
	rm -rf ${DDIR}/${dir}; ln -s ${.CURDIR}/${dir} ${DDIR}
.endfor

.include <bsd.prog.mk>

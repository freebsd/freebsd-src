#	@(#)Makefile	5.2 (Berkeley) 5/6/91
#
# Grungy obj stuff
.if exists(../strfile/obj)
SOBJ=obj
.else
SOBJ=.
.endif

# Get around symlink problem - CVS doesn't like symlinks, replace
# that problem with a settable option. 
FOSRC?=fortunes-o.real.rot13
FOOBJ=fortunes-o

DATFILES=fortunes.dat startrek.dat zippy.dat
CLEANFILES+=${DATFILES}

install: ${DATFILES} ${FOOBJ}.dat
	(cd ${.CURDIR} && install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${DATFILES:R} ${DESTDIR}/usr/share/games/fortune \
	    && install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${FOSRC} ${DESTDIR}/usr/share/games/fortune/${FOOBJ})
	install -o ${BINOWN} -g ${BINGRP} -m 444 ${DATFILES} \
	    ${DESTDIR}/usr/share/games/fortune
	install -o ${BINOWN} -g ${BINGRP} -m 444 ${FOOBJ}.dat \
	    ${DESTDIR}/usr/share/games/fortune

fortunes-o.dat: ${FOSRC}
	${.CURDIR}/../strfile/${SOBJ}/strfile -rsx \
	    ${.CURDIR}/${FOSRC} ${.TARGET}

fortunes.dat startrek.dat zippy.dat: ${.TARGET:R}
	${.CURDIR}/../strfile/${SOBJ}/strfile -rs \
	    ${.CURDIR}/${.TARGET:R} ${.TARGET}

.include <bsd.prog.mk>

#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

MANGRP?=	bin
MANOWN?=	bin
MANMODE?=	444

MANDIR?=	/usr/share/man/man
MANSRC?=	${.CURDIR}
MINSTALL=	install ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

maninstall:
.if defined(MAN1) && !empty(MAN1)
	(cd ${MANSRC}; ${MINSTALL} ${MAN1} ${DESTDIR}${MANDIR}1${MANSUBDIR})
.endif
.if defined(MAN2) && !empty(MAN2)
	(cd ${MANSRC}; ${MINSTALL} ${MAN2} ${DESTDIR}${MANDIR}2${MANSUBDIR})
.endif
.if defined(MAN3) && !empty(MAN3)
	(cd ${MANSRC}; ${MINSTALL} ${MAN3} ${DESTDIR}${MANDIR}3${MANSUBDIR})
.endif
.if defined(MAN3F) && !empty(MAN3F)
	(cd ${MANSRC}; ${MINSTALL} ${MAN3F} ${DESTDIR}${MANDIR}3f${MANSUBDIR})
.endif
.if defined(MAN4) && !empty(MAN4)
	(cd ${MANSRC}; ${MINSTALL} ${MAN4} ${DESTDIR}${MANDIR}4${MANSUBDIR})
.endif
.if defined(MAN5) && !empty(MAN5)
	(cd ${MANSRC}; ${MINSTALL} ${MAN5} ${DESTDIR}${MANDIR}5${MANSUBDIR})
.endif
.if defined(MAN6) && !empty(MAN6)
	(cd ${MANSRC}; ${MINSTALL} ${MAN6} ${DESTDIR}${MANDIR}6${MANSUBDIR})
.endif
.if defined(MAN7) && !empty(MAN7)
	(cd ${MANSRC}; ${MINSTALL} ${MAN7} ${DESTDIR}${MANDIR}7${MANSUBDIR})
.endif
.if defined(MAN8) && !empty(MAN8)
	(cd ${MANSRC}; ${MINSTALL} ${MAN8} ${DESTDIR}${MANDIR}8${MANSUBDIR})
.endif
.if defined(MLINKS) && !empty(MLINKS)
	@set ${MLINKS}; \
	while test $$# -ge 2; do \
		name=$$1; \
		shift; \
		sect=`expr $$name : '.*\.\([^.]*\)'`; \
		dir=${DESTDIR}${MANDIR}$$sect; \
		l=$${dir}${MANSUBDIR}/$$name; \
		name=$$1; \
		shift; \
		sect=`expr $$name : '.*\.\([^.]*\)'`; \
		dir=${DESTDIR}${MANDIR}$$sect; \
		t=$${dir}${MANSUBDIR}/$$name; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done; true
.endif

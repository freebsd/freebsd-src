#	$Id: bsd.man.mk,v 1.9 1996/03/24 00:08:02 wosch Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

MANSRC?=	${.CURDIR}
MINSTALL=	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

MCOMPRESS=	gzip -c
ZEXTENSION=	.gz

SECTIONS=	1 2 3 3f 4 5 6 7 8 9

.undef _MANPAGES
.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
.SUFFIXES: .${sect}
.PATH.${sect}: ${MANSRC}
_MANPAGES+= ${MAN${sect}}
.endif
.endfor

all-man: ${MANDEPEND}

.if defined(NOMANCOMPRESS)

COPY=		-c
ZEXT=

.else

ZEXT=		${ZEXTENSION}

.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
CLEANFILES+=	${MAN${sect}:T:S/$/${ZEXTENSION}/g}
.for page in ${MAN${sect}}
.for target in ${page:T:S/$/${ZEXTENSION}/}
all-man: ${target}
${target}: ${page}
	${MCOMPRESS} ${.ALLSRC} > ${.TARGET}
.endfor
.endfor
.endif
.endfor

.endif

maninstall::
.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
maninstall:: ${MAN${sect}}
.if defined(NOMANCOMPRESS)
	${MINSTALL} ${.ALLSRC} ${DESTDIR}${MANDIR}${sect}${MANSUBDIR}
.else
	${MINSTALL} ${.ALLSRC:T:S/$/${ZEXTENSION}/g} \
		${DESTDIR}${MANDIR}${sect}${MANSUBDIR}
.endif
.endif
.endfor

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
		${ECHO} $${t}${ZEXT} -\> $${l}${ZEXT}; \
		rm -f $${t} $${t}${ZEXTENSION}; \
		ln $${l}${ZEXT} $${t}${ZEXT}; \
	done; true
.endif

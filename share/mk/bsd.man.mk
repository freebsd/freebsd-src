#	$Id: bsd.man.mk,v 1.10 1996/04/09 23:10:19 wosch Exp $

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
	@set `echo ${MLINKS} " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
	while : ; do \
		case $$# in \
			0) break;; \
			[123]) echo "warn: empty MLINK: $$1 $$2 $$3"; break;; \
		esac; \
		name=$$1; shift; sect=$$1; shift; \
		l=${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}/$$name; \
		name=$$1; shift; sect=$$1; shift; \
		t=${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}/$$name; \
		${ECHO} $${t}${ZEXT} -\> $${l}${ZEXT}; \
		rm -f $${t} $${t}${ZEXTENSION}; \
		ln $${l}${ZEXT} $${t}${ZEXT}; \
	done
.endif

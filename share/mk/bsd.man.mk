#	$Id: bsd.man.mk,v 1.4 1994/12/28 03:50:51 ache Exp $

MANGRP?=	bin
MANOWN?=	bin
MANMODE?=	444

MANDIR?=	/usr/share/man/man
MANSRC?=	${.CURDIR}
MINSTALL=	${INSTALL}  ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

MCOMPRESS=	gzip -f
ZEXTENSION=	.gz
.if !defined(NOMANCOMPRESS)
ZEXT=		${ZEXTENSION}
.else
ZEXT=
.endif

MANALL=		${MAN1} ${MAN2} ${MAN3} ${MAN3F} ${MAN4} ${MAN5}	\
		${MAN6} ${MAN7} ${MAN8}

.if !defined(NOMANCOMPRESS)
.for page in ${MANALL}
${page:T}${ZEXTENSION}:	${MANDEPEND} ${page}
	if [ -f ${page} ]; then \
		${MCOMPRESS} < ${page} > ${.TARGET}; \
	else \
		${MCOMPRESS} < ${.CURDIR}/${page} > ${.TARGET}; \
	fi

CLEANFILES+=	${page:T}${ZEXTENSION}
.endfor

.for page in ${MAN1}
COMP1+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN2}
COMP2+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN3}
COMP3+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN3F}
COMP3F+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN4}
COMP4+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN5}
COMP5+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN6}
COMP6+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN7}
COMP7+=	${page:T}${ZEXTENSION}
.endfor
.for page in ${MAN8}
COMP8+=	${page:T}${ZEXTENSION}
.endfor

all-man:	${COMP1} ${COMP2} ${COMP3} ${COMP3F} ${COMP4} \
		${COMP5} ${COMP6} ${COMP7} ${COMP8}
.else
all-man:	${MANDEPEND}
.endif

maninstall:
.for sect in 1 2 3 3F 4 5 6 7 8
.if defined(MAN${sect}) && !empty(MAN${sect})
.if defined(NOMANCOMPRESS)
	(cd ${MANSRC}; \
	 ${MINSTALL} ${MAN${sect}} ${DESTDIR}${MANDIR}${sect:S/F/f/}${MANSUBDIR})
.else
	${MINSTALL} ${COMP${sect}} ${DESTDIR}${MANDIR}${sect:S/F/f/}${MANSUBDIR}
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
		rm -f $${t}${ZEXTENSION}; \
		rm -f $${t}; \
		ln $${l}${ZEXT} $${t}${ZEXT}; \
	done; true
.endif

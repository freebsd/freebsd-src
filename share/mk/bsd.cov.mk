# $FreeBSD$

.include <bsd.own.mk>

FILESGROUPS?=	FILES

GCNOS=		${COV_OBJS:.o=.gcno}

.if !empty(GCNOS)
FILESGROUPS+=	GCNOS
CLEANFILES+=	${GCNOS}

.for _gcno in ${GCNOS}
_gcno_dir=	${COVERAGEDIR}${_gcno:H:tA}
GCNOSDIR_${_gcno:T}=	${_gcno_dir}
.if !target(${DESTDIR}${_gcno_dir})
beforeinstall: ${DESTDIR}${_gcno_dir}
${DESTDIR}${_gcno_dir}:
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},coverage} -d ${.TARGET}
.endif
.endfor
.endif

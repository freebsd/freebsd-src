# $FreeBSD$

.include <bsd.own.mk>

FILESGROUPS?=	FILES
FILESGROUPS+=	GCNOS
_GCNO_FILES=	${OBJS:.o=.gcno}
CLEANFILES+=	${_GCNO_FILES}
GCNOS+=		${_GCNO_FILES}

.for _gcno_file in ${_GCNO_FILES}
_gcno_dir=	${COVERAGEDIR}${_gcno_file:tA:H}
_gcno_fulldir=	${DESTDIR}${_gcno_dir}
GCNOSDIR_${_gcno_file:T}=	${_gcno_dir}

.if !target(${_gcno_fulldir})
beforeinstall: ${_gcno_fulldir}
${_gcno_fulldir}:
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},coverage} -d ${.TARGET}
.endif
.endfor

# $FreeBSD$

.include <bsd.own.mk>

.if make(*clean) || make(*install)

FILESGROUPS?=	FILES

cov_objs_no_suffixes=	${COV_OBJS:R}
.for src in ${COV_SRCS:R}
.if ${cov_objs_no_suffixes:M${src}}
GCNOS+=	${src}.gcno
.endif
.endfor

.if !empty(GCNOS)
GCNOS:=		${GCNOS:O:u}
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

.endif

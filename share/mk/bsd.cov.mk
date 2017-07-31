# $FreeBSD$

.ifdef notyet
.include <bsd.own.mk>

FILESGROUPS?=	FILES
_GCNOGROUPS=

.for _obj in ${OBJS}
# XXX (ngie): this is pretty hamfisted
.if !empty(SRCS:T:M${obj:.o=.asm}) || !empty(SRCS:T:M${obj:.o=.s})
_gcno=		${_obj:.o=.gcno}
_gcno_prefix=	${_gcno:H:tA}
_gcno_dir=	${COVERAGEDIR}${_gcno_prefix}

_GCNO_GROUPS+=	${_gcno_prefix}
CLEANFILES+=	${_gcno}
${_gcno_prefix}+=	${_gcno}
${_gcno_prefix}DIR=	${_gcno_dir}
${_gcno}: ${_obj}
.endif
.endfor

_GCNO_GROUPS:=	${_GCNO_GROUPS:O:u}
FILESGROUPS+=	${_GCNO_GROUPS}
.for _gcno_group in ${_GCNO_GROUPS}
beforeinstall: ${DESTDIR}${${_gcno_group}DIR}
${DESTDIR}${${_gcno_group}DIR}:
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},coverage} -d ${.TARGET}
.endfor
.endif

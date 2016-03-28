# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.incs.mk cannot be included directly.
.endif

.if !defined(NO_INCS) && ${MK_TOOLCHAIN} != "no"

INCSGROUPS?=	INCS

_INCSGROUPS=	${INCSGROUPS:C,[/*],_,g}

.if !target(buildincludes)
.for group in ${_INCSGROUPS}
buildincludes: ${${group}}
.endfor
.endif

all: buildincludes

.if !target(installincludes)
.for group in ${_INCSGROUPS}
.if defined(${group}) && !empty(${group})

${group}OWN?=	${BINOWN}
${group}GRP?=	${BINGRP}
${group}MODE?=	${NOBINMODE}
${group}DIR?=	${INCLUDEDIR}

_${group}INCS=
.for header in ${${group}}
.if defined(${group}OWN_${header:T}) || defined(${group}GRP_${header:T}) || \
    defined(${group}MODE_${header:T}) || defined(${group}DIR_${header:T}) || \
    defined(${group}NAME_${header:T})
${group}OWN_${header:T}?=	${${group}OWN}
${group}GRP_${header:T}?=	${${group}GRP}
${group}MODE_${header:T}?=	${${group}MODE}
${group}DIR_${header:T}?=	${${group}DIR}
.if defined(${group}NAME)
${group}NAME_${header:T}?=	${${group}NAME}
.else
${group}NAME_${header:T}?=	${header:T}
.endif
installincludes: _${group}INS_${header:T}
_${group}INS_${header:T}: ${header}
	${INSTALL} -C -o ${${group}OWN_${.ALLSRC:T}} \
	    -g ${${group}GRP_${.ALLSRC:T}} -m ${${group}MODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR_${.ALLSRC:T}}/${${group}NAME_${.ALLSRC:T}}
.else
_${group}INCS+= ${header}
.endif
.endfor
.if !empty(_${group}INCS)
installincludes: _${group}INS
_${group}INS: ${_${group}INCS}
.if defined(${group}NAME)
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}/${${group}NAME}
.else
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}
.endif
.endif

.endif # defined(${group}) && !empty(${group})
.endfor

.if defined(INCSLINKS) && !empty(INCSLINKS)
installincludes:
	@set ${INCSLINKS}; \
	while test $$# -ge 2; do \
		l=$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		${INSTALL_SYMLINK} $$l $$t; \
	done; true
.endif
.endif # !target(installincludes)

realinstall: installincludes
.ORDER: beforeinstall installincludes

.endif # !defined(NO_INCS) && ${MK_TOOLCHAIN} != "no"

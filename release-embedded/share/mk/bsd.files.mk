# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.files.mk cannot be included directly.
.endif

FILESGROUPS?=	FILES

.if !target(buildfiles)
.for group in ${FILESGROUPS}
buildfiles: ${${group}}
.endfor
.endif

all: buildfiles

.for group in ${FILESGROUPS}
.if defined(${group}) && !empty(${group})
installfiles: installfiles-${group}

${group}OWN?=	${SHAREOWN}
${group}GRP?=	${SHAREGRP}
${group}MODE?=	${SHAREMODE}
${group}DIR?=	${BINDIR}

_${group}FILES=
.for file in ${${group}}
.if defined(${group}OWN_${file:T}) || defined(${group}GRP_${file:T}) || \
    defined(${group}MODE_${file:T}) || defined(${group}DIR_${file:T}) || \
    defined(${group}NAME_${file:T})
${group}OWN_${file:T}?=	${${group}OWN}
${group}GRP_${file:T}?=	${${group}GRP}
${group}MODE_${file:T}?=	${${group}MODE}
${group}DIR_${file:T}?=	${${group}DIR}
.if defined(${group}NAME)
${group}NAME_${file:T}?=	${${group}NAME}
.else
${group}NAME_${file:T}?=	${file:T}
.endif
installfiles-${group}: _${group}INS_${file:T}
_${group}INS_${file:T}: ${file}
	${INSTALL} -o ${${group}OWN_${.ALLSRC:T}} \
	    -g ${${group}GRP_${.ALLSRC:T}} -m ${${group}MODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR_${.ALLSRC:T}}/${${group}NAME_${.ALLSRC:T}}
.else
_${group}FILES+= ${file}
.endif
.endfor
.if !empty(_${group}FILES)
installfiles-${group}: _${group}INS
_${group}INS: ${_${group}FILES}
.if defined(${group}NAME)
	${INSTALL} -o ${${group}OWN} -g ${${group}GRP} \
	    -m ${${group}MODE} ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR}/${${group}NAME}
.else
	${INSTALL} -o ${${group}OWN} -g ${${group}GRP} \
	    -m ${${group}MODE} ${.ALLSRC} ${DESTDIR}${${group}DIR}
.endif
.endif

.endif # defined(${group}) && !empty(${group})
.endfor

realinstall: installfiles
.ORDER: beforeinstall installfiles

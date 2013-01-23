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

.if !target(installfiles)
.for group in ${FILESGROUPS}
.if defined(${group}) && !empty(${group})

${group}OWN?=	${SHAREOWN}
${group}GRP?=	${SHAREGRP}
${group}MODE?=	${SHAREMODE}
${group}DIR?=	${BINDIR}
.if !make(buildincludes)
STAGE_SETS+=	${group}
.endif
STAGE_DIR.${group}= ${STAGE_OBJTOP}${${group}DIR}
STAGE_SYMLINKS_DIR.${group}= ${STAGE_OBJTOP}

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
.if !make(buildincludes)
STAGE_AS_SETS+=	${group}
.endif
STAGE_AS_${file:T}= ${${group}NAME_${file:T}}
stage_as.${group}: ${file}

installfiles: _${group}INS_${file:T}
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
stage_files.${group}: ${_${group}FILES}

installfiles: _${group}INS
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

.endif # !target(installfiles)

realinstall: installfiles
.ORDER: beforeinstall installfiles

.if ${MK_STAGING} != "no"
.if !empty(STAGE_SETS)
buildfiles: stage_files
.if !empty(STAGE_AS_SETS)
buildfiles: stage_as
.endif
.endif
.endif

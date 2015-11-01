# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.files.mk cannot be included directly.
.endif

.if !target(__<bsd.files.mk>__)
__<bsd.files.mk>__:

FILESGROUPS?=	FILES

.for group in ${FILESGROUPS}
# Add in foo.yes and remove duplicates from all the groups
${${group}}:= ${${group}} ${${group}.yes}
${${group}}:= ${${group}:O:u}
buildfiles: ${${group}}
.endfor

all: buildfiles

.for group in ${FILESGROUPS}
.if defined(${group}) && !empty(${group})
installfiles: installfiles-${group}

${group}OWN?=	${SHAREOWN}
${group}GRP?=	${SHAREGRP}
${group}MODE?=	${SHAREMODE}
${group}DIR?=	${BINDIR}
.if !make(buildincludes)
STAGE_SETS+=	${group}
.endif
STAGE_DIR.${group}= ${STAGE_OBJTOP}${${group}DIR}

_${group}FILES=
.for file in ${${group}}
.if defined(${group}OWN_${file:T}) || defined(${group}GRP_${file:T}) || \
    defined(${group}MODE_${file:T}) || defined(${group}DIR_${file:T}) || \
    defined(${group}NAME_${file:T}) || defined(${group}NAME)
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
STAGE_AS_SETS+=	${file:T}
.endif
STAGE_AS_${file:T}= ${${group}NAME_${file:T}}
# XXX {group}OWN,GRP,MODE
STAGE_DIR.${file:T}= ${STAGE_OBJTOP}${${group}DIR_${file:T}}
stage_as.${file:T}: ${file}

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
stage_files.${group}: ${_${group}FILES}

installfiles-${group}: _${group}INS
_${group}INS: ${_${group}FILES}
.if defined(${group}NAME)
	${INSTALL} -o ${${group}OWN} -g ${${group}GRP} \
	    -m ${${group}MODE} ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR}/${${group}NAME}
.else
	${INSTALL} -o ${${group}OWN} -g ${${group}GRP} \
	    -m ${${group}MODE} ${.ALLSRC} ${DESTDIR}${${group}DIR}/
.endif
.endif

.endif # defined(${group}) && !empty(${group})
.endfor

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

.endif # !target(__<bsd.files.mk>__)

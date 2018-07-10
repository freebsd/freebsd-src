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

.if !defined(_SKIP_BUILD)
all: buildfiles
.endif

.for group in ${FILESGROUPS}
.if defined(${group}) && !empty(${group})
installfiles: installfiles-${group}

${group}OWN?=	${SHAREOWN}
${group}GRP?=	${SHAREGRP}
.if ${MK_INSTALL_AS_USER} == "yes"
${group}OWN=	${SHAREOWN}
${group}GRP=	${SHAREGRP}
.endif
${group}MODE?=	${SHAREMODE}
${group}DIR?=	${BINDIR}
STAGE_SETS+=	${group:C,[/*],_,g}
STAGE_DIR.${group:C,[/*],_,g}= ${STAGE_OBJTOP}${${group}DIR}

.if defined(NO_ROOT)
.if !defined(${group}TAGS) || ! ${${group}TAGS:Mpackage=*}
${group}TAGS+=		package=${${group}PACKAGE:Uruntime}
.endif
${group}TAG_ARGS=	-T ${${group}TAGS:[*]:S/ /,/g}
.endif


_${group}FILES=
.for file in ${${group}}
.if defined(${group}OWN_${file:T}) || defined(${group}GRP_${file:T}) || \
    defined(${group}MODE_${file:T}) || defined(${group}DIR_${file:T}) || \
    defined(${group}NAME_${file:T}) || defined(${group}NAME)
${group}OWN_${file:T}?=	${${group}OWN}
${group}GRP_${file:T}?=	${${group}GRP}
.if ${MK_INSTALL_AS_USER} == "yes"
${group}OWN_${file:T}=	${SHAREOWN}
${group}GRP_${file:T}=	${SHAREGRP}
.endif
${group}MODE_${file:T}?=	${${group}MODE}
${group}DIR_${file:T}?=	${${group}DIR}
.if defined(${group}NAME)
${group}NAME_${file:T}?=	${${group}NAME}
.else
${group}NAME_${file:T}?=	${file:T}
.endif
STAGE_AS_SETS+=	${file:T}
STAGE_AS_${file:T}= ${${group}NAME_${file:T}}
# XXX {group}OWN,GRP,MODE
STAGE_DIR.${file:T}= ${STAGE_OBJTOP}${${group}DIR_${file:T}}
stage_as.${file:T}: ${file}

installfiles-${group}: installdirs-${group} _${group}INS_${file:T}
_${group}INS_${file:T}: ${file}
	${INSTALL} ${${group}TAG_ARGS} -o ${${group}OWN_${.ALLSRC:T}} \
	    -g ${${group}GRP_${.ALLSRC:T}} -m ${${group}MODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR_${.ALLSRC:T}}/${${group}NAME_${.ALLSRC:T}}
.else
_${group}FILES+= ${file}
.endif
.endfor


installdirs-${group}:
	@${ECHO} installing dirs ${group}DIR ${${group}DIR}
.for dir in ${${group}DIR}
.if defined(NO_ROOT)
	${INSTALL} ${${group}TAG_ARGS} -d ${DESTDIR}${dir}
.else
	${INSTALL} ${${group}TAG_ARGS} -d -o ${DIROWN} -g ${DIRGRP} \
		-m ${DIRMODE} ${DESTDIR}${dir}
.endif
.endfor


.if !empty(_${group}FILES)
stage_files.${group}: ${_${group}FILES}

installfiles-${group}: installdirs-${group} _${group}INS
_${group}INS: ${_${group}FILES}
.if defined(${group}NAME)
	${INSTALL} ${${group}TAG_ARGS} -o ${${group}OWN} -g ${${group}GRP} \
	    -m ${${group}MODE} ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR}/${${group}NAME}
.else
	${INSTALL} ${${group}TAG_ARGS} -o ${${group}OWN} -g ${${group}GRP} \
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

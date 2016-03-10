# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.conf.mk cannot be included directly.
.endif

CONFGROUPS?=	CONFS

_CONFGROUPS=	${CONFGROUPS:C,[/*],_,g}

.if !target(buildconfig)
.for group in ${_CONFGROUPS}
buildconfig: ${${group}}
.endfor
.endif

.if !defined(_SKIP_BUILD)
all: buildconfig
.endif

.if !target(installconfig)
.for group in ${_CONFGROUPS}
.if defined(${group}) && !empty(${group})

${group}OWN?=	${SHAREOWN}
${group}GRP?=	${SHAREGRP}
${group}MODE?=	${CONFMODE}
${group}DIR?=	${CONFDIR}
STAGE_SETS+=	${group}
STAGE_DIR.${group}= ${STAGE_OBJTOP}${${group}DIR}

_${group}CONFS=
.for cnf in ${${group}}
.if defined(${group}OWN_${cnf:T}) || defined(${group}GRP_${cnf:T}) || \
    defined(${group}MODE_${cnf:T}) || defined(${group}DIR_${cnf:T}) || \
    defined(${group}NAME_${cnf:T}) || defined(${group}NAME)
${group}OWN_${cnf:T}?=	${${group}OWN}
${group}GRP_${cnf:T}?=	${${group}GRP}
${group}MODE_${cnf:T}?=	${${group}MODE}
${group}DIR_${cnf:T}?=	${${group}DIR}
.if defined(${group}NAME)
${group}NAME_${cnf:T}?=	${${group}NAME}
.else
${group}NAME_${cnf:T}?=	${cnf:T}
.endif
STAGE_AS_SETS+= ${cnf:T}
STAGE_AS_${cnf:T}= ${${group}NAME_${cnf:T}}
# XXX {group}OWN,GRP,MODE
STAGE_DIR.${cnf:T}= ${STAGE_OBJTOP}${${group}DIR_${cnf:T}}
stage_as.${cnf:T}: ${cnf}

installconfig: _${group}INS_${cnf:T}
_${group}INS_${cnf:T}: ${cnf}
	${INSTALL} -C -o ${${group}OWN_${.ALLSRC:T}} \
	    -g ${${group}GRP_${.ALLSRC:T}} -m ${${group}MODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR_${.ALLSRC:T}}/${${group}NAME_${.ALLSRC:T}}
.else
_${group}CONFS+= ${cnf}
.endif
.endfor
.if !empty(_${group}CONFS)
stage_files.${group}: ${_${group}CONFS}

installconfig: _${group}INS
_${group}INS: ${_${group}CONFS}
.if defined(${group}NAME)
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}/${${group}NAME}
.else
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}/
.endif
.endif

.endif # defined(${group}) && !empty(${group})
.endfor

.endif # !target(installconfig)

.if ${MK_STAGING} != "no"
.if !empty(STAGE_SETS)
buildconfig: stage_files
.if !empty(STAGE_AS_SETS)
buildconfig: stage_as
.endif
.endif
.endif


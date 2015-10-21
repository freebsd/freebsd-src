# $Id: scripts.mk,v 1.2 2006/11/09 01:55:18 sjg Exp $

.include <init.mk>

.if defined(SCRIPTS) 

all:	${SCRIPTS}

.PHONY:		scriptsinstall
install:	scriptsinstall

.if !target(scriptsinstall)
SCRIPTSDIR?=	${BINDIR}
SCRIPTSOWN?=	${BINOWN}
SCRIPTSGRP?=	${BINGRP}
SCRIPTSMODE?=	${BINMODE}

# how we get script name from src
SCRIPTSNAME_MOD?=T:R

script_targets= ${SCRIPTS:@s@${DESTDIR}${SCRIPTSDIR_$s:U${SCRIPTSDIR}}/${SCRIPTSNAME_$s:U${s:${SCRIPTSNAME_MOD}}}@}

scriptsinstall:: ${script_targets}

.PRECIOUS: ${script_targets}
.if !defined(UPDATE)
.PHONY: ${script_targets}
.endif

INSTALL_FLAGS?= ${RENAME} ${PRESERVE} ${COPY} ${INSTPRIV} \
	-o ${OWN_${.TARGET:T}:U${SCRIPTSOWN}} \
	-g ${GRP_${.TARGET:T}:U${SCRIPTSGRP}} \
	-m ${MODE_${.TARGET:T}:U${SCRIPTSMODE}}

__SCRIPTINSTALL_USE: .USE
	${INSTALL} ${INSTALL_FLAGS_${.TARGET:T}:U${INSTALL_FLAGS}} \
	    ${.ALLSRC} ${.TARGET}

.for s in ${SCRIPTS}
.if !defined(BUILD) && !make(all) && !make(${s})
${DESTDIR}${SCRIPTSDIR_$s:U${SCRIPTSDIR}}/${SCRIPTSNAME_$s:U${s:${SCRIPTSNAME_MOD}}}:	.MADE
.endif
${DESTDIR}${SCRIPTSDIR_$s:U${SCRIPTSDIR}}/${SCRIPTSNAME_$s:U${s:${SCRIPTSNAME_MOD}}}:	${s} __SCRIPTINSTALL_USE
.endfor
.endif

.endif

.if !target(scriptsinstall)
scriptsinstall::
.endif


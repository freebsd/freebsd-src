# $Id: final.mk,v 1.8 2017/05/07 20:30:08 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

# provide a hook for folk who want to do scary stuff
.-include <${.CURDIR:H}/Makefile-final.inc>

.if ${MK_STAGING} == "yes"
.include <meta.stage.mk>
.elif !empty(STAGE)
.-include <stage.mk>
.endif

.-include <local.final.mk>

.if empty(_SKIP_BUILD)
install: realinstall
.endif
realinstall:

.endif

# $Id: final.mk,v 1.10 2021/12/08 05:56:50 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

# provide a hook for folk who want to do scary stuff
.-include <${.CURDIR:H}/Makefile-final.inc>

.-include <local.final.mk>

.if ${MK_STAGING} == "yes"
.include <meta.stage.mk>
.elif !empty(STAGE)
.-include <stage.mk>
.endif

.if empty(_SKIP_BUILD)
install: realinstall
.endif
realinstall:

.endif

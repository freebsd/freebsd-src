# $Id: final.mk,v 1.6 2016/04/05 15:58:37 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

# provide a hook for folk who want to do scary stuff
.-include <${.CURDIR:H}/Makefile-final.inc>

.if !empty(STAGE)
.-include <stage.mk>
.endif

.-include <local.final.mk>
.endif

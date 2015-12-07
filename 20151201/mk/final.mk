# $Id: final.mk,v 1.5 2011/03/11 05:22:38 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

# provide a hook for folk who want to do scary stuff
.-include "${.CURDIR}/../Makefile-final.inc"

.if !empty(STAGE)
.-include <stage.mk>
.endif

.-include <local.final.mk>
.endif

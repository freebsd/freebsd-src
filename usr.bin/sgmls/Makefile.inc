#
# Bmakefile for rast
#
# $Id: Makefile.inc,v 1.3 1996/06/19 20:36:35 jkh Exp $
#

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if exists(${.OBJDIR}/../libsgmls)
LIBSGMLS=       ${.OBJDIR}/../libsgmls/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif


#
# Bmakefile for rast
#
# $Id: Makefile.inc,v 1.4 1996/06/24 04:24:39 jkh Exp $
#

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if exists(${.OBJDIR}/../libsgmls)
LIBSGMLS=       ${.OBJDIR}/../libsgmls/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif


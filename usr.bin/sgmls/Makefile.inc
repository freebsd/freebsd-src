#
# Bmakefile for rast
#
# $Id: Makefile.inc,v 1.2 1996/06/04 19:08:04 jfieber Exp $
#

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if exists(${.CURDIR}/../libsgmls/obj)
LIBSGMLS=       ${.CURDIR}/../libsgmls/obj/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif


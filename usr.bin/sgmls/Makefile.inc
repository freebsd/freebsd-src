#
# Bmakefile for rast
#
# $Id: Makefile.inc,v 1.1.1.1.4.1 1996/06/19 20:55:10 jkh Exp $
#

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if exists(${.CURDIR}/../libsgmls/obj)
LIBSGMLS=       ${.CURDIR}/../libsgmls/obj/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif


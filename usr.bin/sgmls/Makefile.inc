#
# Bmakefile for rast
#
# $Id$
#

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if exists(${.CURDIR}/../libsgmls/obj)
LIBSGMLS=       ${.CURDIR}/../libsgmls/obj/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif

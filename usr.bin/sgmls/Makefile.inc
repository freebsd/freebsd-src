#
# Bmakefile for rast
#
# $id$
#

.include "${.CURDIR}/../../Makefile.inc"

.if exists(${.CURDIR}/../libsgmls/obj)
LIBSGMLS=       ${.CURDIR}/../libsgmls/obj/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif
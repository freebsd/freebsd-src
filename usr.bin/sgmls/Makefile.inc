#
# Bmakefile for rast
#
# $FreeBSD$
#

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if exists(${.OBJDIR}/../libsgmls)
LIBSGMLS=       ${.OBJDIR}/../libsgmls/libsgmls.a
.else
LIBSGMLS=       ${.CURDIR}/../libsgmls/libsgmls.a
.endif


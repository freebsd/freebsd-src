# $FreeBSD: src/lib/msun/ia64/Makefile.inc,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map

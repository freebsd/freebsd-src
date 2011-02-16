# $FreeBSD: src/lib/msun/ia64/Makefile.inc,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map

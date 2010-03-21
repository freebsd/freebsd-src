# $FreeBSD: src/lib/msun/ia64/Makefile.inc,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map

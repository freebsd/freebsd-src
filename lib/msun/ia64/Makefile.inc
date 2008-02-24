# $FreeBSD: src/lib/msun/ia64/Makefile.inc,v 1.3 2006/03/27 23:59:45 deischen Exp $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map

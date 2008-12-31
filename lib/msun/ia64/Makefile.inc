# $FreeBSD: src/lib/msun/ia64/Makefile.inc,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map

# $FreeBSD: src/lib/msun/ia64/Makefile.inc,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map

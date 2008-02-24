# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3 2006/03/27 23:59:45 deischen Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

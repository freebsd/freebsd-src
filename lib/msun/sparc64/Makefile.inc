# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

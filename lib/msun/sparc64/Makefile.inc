# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

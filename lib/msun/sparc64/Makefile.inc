# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

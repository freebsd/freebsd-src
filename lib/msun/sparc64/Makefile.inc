# $FreeBSD: src/lib/msun/sparc64/Makefile.inc,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map

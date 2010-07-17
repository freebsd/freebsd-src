#	$NetBSD: Makefile.inc,v 1.7 2005/09/17 11:49:39 tsutsui Exp $
# $FreeBSD: src/lib/libc/mips/Makefile.inc,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

MDSRCS+= machdep_ldisd.c
SYM_MAPS+= ${.CURDIR}/mips/Symbol.map

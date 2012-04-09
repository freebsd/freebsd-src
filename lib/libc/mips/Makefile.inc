#	$NetBSD: Makefile.inc,v 1.7 2005/09/17 11:49:39 tsutsui Exp $
# $FreeBSD: src/lib/libc/mips/Makefile.inc,v 1.3.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

CFLAGS+=-DSOFTFLOAT

MDSRCS+= machdep_ldisd.c
SYM_MAPS+= ${.CURDIR}/mips/Symbol.map

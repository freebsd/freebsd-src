#	$Id: Makefile.bsd,v 1.2 1994/04/25 06:48:25 paulus Exp $

PROG=	pppstats
SRCS=	pppstats.c
MAN8=	pppstats.8

LDADD=	-lutil -lkvm
BINMODE=2555
BINGRP=	kmem
BINDIR=	/usr/sbin

.include <bsd.prog.mk>

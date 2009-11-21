# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

LIBFIFOLOG= ${.OBJDIR}/../lib/libfifolog.a

WARNS?=	6

#LINT=	flint
#LINTFLAGS= ${.CURDIR}/../flint.lnt -I/usr/include

.include "../Makefile.inc"

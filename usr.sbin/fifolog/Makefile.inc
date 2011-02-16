# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIBFIFOLOG= ${.OBJDIR}/../lib/libfifolog.a

WARNS?=	6

#LINT=	flint
#LINTFLAGS= ${.CURDIR}/../flint.lnt -I/usr/include

.include "../Makefile.inc"

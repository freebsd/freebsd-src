# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

LIBFIFOLOG =	${.OBJDIR}/../lib/libfifolog.a


WARNS ?=	6

# CFLAGS	+=	-O0 -g

# LINT =		flint
# LINTFLAGS =	${.CURDIR}/../flint.lnt -I/usr/include 

.include "../Makefile.inc"

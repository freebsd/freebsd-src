#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/nls

SRCS+=	msgcat.c

MAN+=	catclose.3 catgets.3 catopen.3

# NOTE: C.msg should not be processed here, it's using as template
#       for translators.

NLSNAME=	libc
NLS+=		ru_RU.KOI8-R

NLSSRCDIR=	${.CURDIR}/nls
.for lang in ${NLS}
NLSSRCFILES_${lang}=${lang}.msg
.endfor

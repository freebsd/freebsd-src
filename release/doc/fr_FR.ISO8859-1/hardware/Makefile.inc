#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/hardware/Makefile.inc,v 1.1.2.2 2003/01/18 18:06:34 gioria Exp $
#      Original revision: 1.1.2.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/hardware/${.CURDIR:T}
.endif

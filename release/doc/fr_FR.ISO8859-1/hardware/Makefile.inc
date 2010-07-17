#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/hardware/Makefile.inc,v 1.2.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#      Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/hardware/${.CURDIR:T}
.endif

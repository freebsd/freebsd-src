#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/installation/Makefile.inc,v 1.2.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#      Original revision: 1.2


.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/installation/${.CURDIR:T}
.endif

#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD$
#      Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/hardware/${.CURDIR:T}
.endif

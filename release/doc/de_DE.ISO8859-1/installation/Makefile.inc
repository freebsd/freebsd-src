# $FreeBSD$
# $FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/installation/Makefile.inc,v 1.2 2002/03/10 18:41:39 ue Exp $
# $Id: Makefile.inc,v 1.4 2002/03/10 15:37:17 ue Exp $
# basiert auf: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=      ${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/de_DE.ISO8859-1/installation/${.CURDIR:T}
.endif

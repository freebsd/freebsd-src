# $FreeBSD$
# $FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/relnotes/Makefile.inc,v 1.2.2.1 2002/03/11 10:51:26 ue Exp $
# $Id: Makefile.inc,v 1.3 2002/03/10 15:37:19 ue Exp $
# basiert auf: 1.1.2.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=       ${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=       ${DOCDIR}/de_DE.ISO8859-1/relnotes/${.CURDIR:T}
.endif


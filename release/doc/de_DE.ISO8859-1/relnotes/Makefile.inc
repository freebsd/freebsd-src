# $FreeBSD: src/release/doc/de_DE.ISO8859-1/relnotes/Makefile.inc,v 1.1.38.1.4.1 2010/06/14 02:09:06 kensmith Exp $
# $FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/relnotes/Makefile.inc,v 1.2 2002/03/10 18:41:45 ue Exp $
# $Id: Makefile.inc,v 1.3 2002/03/10 15:37:19 ue Exp $
# basiert auf: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=       ${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=       ${DOCDIR}/de_DE.ISO8859-1/relnotes/${.CURDIR:T}
.endif


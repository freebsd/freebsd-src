# The FreeBSD Russian Documentation Project
#
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/early-adopter/Makefile,v 1.1 2004/06/28 13:24:45 den Exp $
# $FreeBSD: src/release/doc/ru_RU.KOI8-R/early-adopter/Makefile,v 1.1 2004/06/28 13:28:15 den Exp $
#
# Original revision: 1.3
#

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/early-adopter
.else
DESTDIR?= ${DOCDIR}/ru_RU.KOI8-R/early-adopter
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?=gz
INSTALL_ONLY_COMPRESSED?=

SRCS+= article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD: src/release/doc/en_US.ISO8859-1/errata/Makefile,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/errata
.else
DESTDIR?= ${DOCDIR}/en_US.ISO8859-1/errata
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

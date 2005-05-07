# The FreeBSD Simplified Chinese Project
# Original Revision: 1.3
#
# $FreeBSD: src/release/doc/zh_CN.GB2312/early-adopter/Makefile,v 1.1 2004/10/15 06:43:44 delphij Exp $

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/early-adopter
.else
DESTDIR?= ${DOCDIR}/zh_CN.GB2312/early-adopter
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?=gz
INSTALL_ONLY_COMPRESSED?=

SRCS+= article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

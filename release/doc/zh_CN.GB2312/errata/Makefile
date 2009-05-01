# Original Revision: 82872
# $FreeBSD: src/release/doc/zh_CN.GB2312/errata/Makefile,v 1.1.16.2.4.1 2009/04/15 03:14:26 kensmith Exp $

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/errata
.else
DESTDIR?= ${DOCDIR}/zh_CN.GB2312/errata
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

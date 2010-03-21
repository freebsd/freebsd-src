# The FreeBSD Simplified Chinese Project
# Original Revision: 164989
#
# $FreeBSD: src/release/doc/zh_CN.GB2312/relnotes/Makefile,v 1.2.2.2.6.1 2010/02/10 00:26:20 kensmith Exp $

RELN_ROOT?= ${.CURDIR}/../..

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes
.else
DESTDIR?=	${DOCDIR}/zh_CN.GB2312/relnotes
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml

URL_RELPREFIX?=	../../../..

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

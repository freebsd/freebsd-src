# The FreeBSD Simplified Chinese Project
#
# Original Revision: 166821
# $FreeBSD: src/release/doc/zh_CN.GB2312/hardware/Makefile,v 1.1.18.2.6.1 2010/02/10 00:26:20 kensmith Exp $

RELN_ROOT?= ${.CURDIR}/../..

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware
.else
DESTDIR?=	${DOCDIR}/zh_CN.GB2312/hardware
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml
SRCS+=	${DEV-AUTODIR}/catalog-auto
SRCS+=	${DEV-AUTODIR}/dev-auto.sgml

CATALOGS+=	-c ${DEV-AUTODIR}/catalog-auto

URL_RELPREFIX?=	../../../..

HWNOTES_MI=	1

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

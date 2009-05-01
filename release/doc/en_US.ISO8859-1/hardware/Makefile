# $FreeBSD: src/release/doc/en_US.ISO8859-1/hardware/Makefile,v 1.10.8.1 2009/04/15 03:14:26 kensmith Exp $

RELN_ROOT?= ${.CURDIR}/../..

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/hardware
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

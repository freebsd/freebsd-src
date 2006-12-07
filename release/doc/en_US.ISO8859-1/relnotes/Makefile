# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../..

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/relnotes/${.CURDIR:T}
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

# The FreeBSD Russian Documentation Project
#
# $FreeBSD$
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/hardware/amd64/Makefile,v 1.3 2005/03/10 11:19:53 den Exp $
#
# Original revision: 1.2

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.xml 
SRCS+=	proc-amd64.xml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.xml
SRCS+=	../common/dev.xml
SRCS+=	${DEV-AUTODIR}/catalog-auto
SRCS+=	${DEV-AUTODIR}/dev-auto.ent
SRCS+= ../../share/xml/dev-auto-ru.ent

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

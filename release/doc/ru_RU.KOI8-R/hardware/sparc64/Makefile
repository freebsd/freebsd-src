# The FreeBSD Russian Documentation Project
#
# $FreeBSD$
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/hardware/sparc64/Makefile,v 1.1 2004/05/27 06:09:43 den Exp $
#
# Original revision: 1.3

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml 
SRCS+=	proc-sparc64.sgml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/dev.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

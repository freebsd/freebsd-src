# $FreeBSD: src/release/doc/ja_JP.eucJP/relnotes/i386/Makefile,v 1.2.2.2 2002/01/01 13:43:51 hrs Exp $
# Original revision: 1.3.2.4

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=    -V %generate-article-toc%

# SGML content
SRCS+=	article.sgml
SRCS+=	../common/relnotes.ent
SRCS+=	../common/new.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

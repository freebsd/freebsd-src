# Original Revision: 1.1
# $FreeBSD: src/release/doc/zh_CN.GB2312/installation/pc98/Makefile,v 1.1 2004/10/15 06:43:46 delphij Exp $

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml
SRCS+=	../common/artheader.sgml
SRCS+=	../common/abstract.sgml
SRCS+=	../common/install.sgml
SRCS+=	../common/layout.sgml
SRCS+=	../common/trouble.sgml
SRCS+=	../common/upgrade.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD: src/release/doc/en_US.ISO8859-1/installation/ia64/Makefile,v 1.1 2002/11/08 18:01:31 bmah Exp $

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

# $FreeBSD: src/release/doc/en_US.ISO8859-1/installation/sparc64/Makefile,v 1.1 2002/03/05 23:22:08 tmm Exp $

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml
SRCS+=	install.sgml
SRCS+=	../common/artheader.sgml
SRCS+=	../common/install.sgml
SRCS+=	../common/layout.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

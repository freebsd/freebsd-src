# The FreeBSD Russian Documentation Project
#
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/installation/sparc64/Makefile,v 1.1 2004/06/24 07:21:48 den Exp $
# $FreeBSD$
#
# Original revision: 1.1
#

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml
SRCS+=	../../../en_US.ISO8859-1/installation/sparc64/install.sgml
SRCS+=	../common/artheader.sgml
SRCS+=	../common/install.sgml
SRCS+=	../common/layout.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

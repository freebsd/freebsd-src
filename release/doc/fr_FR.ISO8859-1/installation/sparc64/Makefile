#
#     The FreeBSD Documentation Project
#     The FreeBSD French Documentation Project
#     $Id$
#
#     $FreeBSD: src/release/doc/fr_FR.ISO8859-1/installation/sparc64/Makefile,v 1.1.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#     Original revision: 1.1
#


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

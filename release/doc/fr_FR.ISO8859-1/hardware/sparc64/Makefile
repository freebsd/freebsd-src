#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/hardware/sparc64/Makefile,v 1.1 2002/09/09 22:09:48 gioria Exp $
#      Original revision: 1.1


RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml 
SRCS+=	proc-sparc64.sgml 
SRCS+=	dev-sparc64.sgml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD$
# $FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/hardware/alpha/Makefile,v 1.3 2002/03/10 18:41:33 ue Exp $
# $Id: Makefile,v 1.6 2002/03/10 15:37:14 ue Exp $
# basiert auf: 1.6

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=    -V %generate-article-toc%

# SGML content
SRCS+=	article.sgml
SRCS+=	proc-alpha.sgml
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/dev.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD$
# $FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/hardware/pc98/Makefile,v 1.1 2002/07/08 18:41:44 ue Exp $
# basiert auf: 1.1

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=    -V %generate-article-toc%

# SGML content
SRCS+=	article.xml 
SRCS+=	proc-pc98.xml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.xml
SRCS+=	../common/dev.xml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD$
# $FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/installation/pc98/Makefile,v 1.1 2002/07/08 18:41:45 ue Exp $
# basiert auf: 1.1

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.xml
SRCS+=	../common/artheader.xml
SRCS+=	../common/install.xml
SRCS+=	../common/layout.xml
SRCS+=	../common/trouble.xml
SRCS+=	../common/upgrade.xml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

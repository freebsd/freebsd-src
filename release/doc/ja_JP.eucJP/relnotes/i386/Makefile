# $FreeBSD$
# Original revision: 1.3.2.2

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

NO_TIDY?=YES

# SGML content
SRCS+=	article.sgml
SRCS+=	../common/relnotes.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/intro.sgml
SRCS+=	../common/new.sgml
SRCS+=	../common/upgrading.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

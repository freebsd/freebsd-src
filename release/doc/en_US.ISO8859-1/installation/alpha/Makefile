# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=
EXTRA_CATALOGS+= ${RELN_ROOT}/share/sgml/catalog

# SGML content
SRCS+=	article.sgml
SRCS+=	../common/artheader.sgml
SRCS+=	../common/install.sgml
SRCS+=	../common/floppies.sgml
SRCS+=	../common/layout.sgml
SRCS+=	../common/trouble.sgml
SRCS+=	../common/upgrade.sgml

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${RELN_ROOT}/share/sgml/release-alpha.dsl
DSLPRINT?=	${RELN_ROOT}/share/sgml/release-alpha.dsl

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

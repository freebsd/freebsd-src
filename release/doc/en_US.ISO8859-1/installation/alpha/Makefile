# $FreeBSD$

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=
EXTRA_CATALOGS+= ../../../share/sgml/catalog

# SGML content
SRCS+=	article.sgml
SRCS+=	../common/artheader.sgml
SRCS+=	../common/install.sgml
SRCS+=	../common/floppies.sgml
SRCS+=	../common/layout.sgml
SRCS+=	../common/trouble.sgml
SRCS+=	../common/upgrade.sgml

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${.CURDIR}/../../../share/sgml/release-alpha.dsl
DSLPRINT?=	${.CURDIR}/../../../share/sgml/release-alpha.dsl

DOC_PREFIX?= /usr/doc
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD$

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=
EXTRA_CATALOGS+= ../../../share/sgml/catalog

# SGML content
SRCS+=	article.sgml
SRCS+=	proc-alpha.sgml
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/dev.sgml
SRCS+=	../common/config.sgml

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${.CURDIR}/../../../share/sgml/release-alpha.dsl
DSLPRINT?=	${.CURDIR}/../../../share/sgml/release-alpha.dsl

DOC_PREFIX?= /usr/doc
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

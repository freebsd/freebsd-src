# $FreeBSD: src/release/doc/share/mk/doc.relnotes.mk,v 1.3.2.5 2001/07/17 00:53:33 bmah Exp $

DOC_PREFIX?= ${RELN_ROOT}/../../../doc

# Find the RELNOTESng document catalogs
EXTRA_CATALOGS+= ${RELN_ROOT}/${LANGCODE}/share/sgml/catalog
EXTRA_CATALOGS+= ${RELN_ROOT}/share/sgml/catalog

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${RELN_ROOT}/share/sgml/default.dsl
DSLPRINT?=	${RELN_ROOT}/share/sgml/default.dsl

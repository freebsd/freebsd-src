# $FreeBSD$

DOC_PREFIX?= ${RELN_ROOT}/../../../doc

# Find the RELNOTESng document catalog
EXTRA_CATALOGS+= ${RELN_ROOT}/share/sgml/catalog

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${RELN_ROOT}/en_US.ISO8859-1/share/sgml/release.dsl
DSLPRINT?=	${RELN_ROOT}/en_US.ISO8859-1/share/sgml/release.dsl

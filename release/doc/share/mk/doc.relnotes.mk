# $FreeBSD$

DOC_PREFIX?= ${RELN_ROOT}/../../../doc

# Find the RELNOTESng document catalogs
EXTRA_CATALOGS+= ${RELN_ROOT}/${LANGCODE}/share/sgml/catalog
EXTRA_CATALOGS+= ${RELN_ROOT}/share/sgml/catalog

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${RELN_ROOT}/share/sgml/default.dsl
DSLPRINT?=	${RELN_ROOT}/share/sgml/default.dsl

#
# Tweakable Makefile variables
#
# INCLUDE_HISTORIC	Used by relnotes document only.  When set,
#			causes all release notes entries to be printed,
#			even those marked as "historic".  If not set
#			(the default), only print "non-historic"
#			release note entries.  To designate a release
#			note entry as "historic", add a role="historic"
#			attribute to the applicable element(s).
#
.if defined(INCLUDE_HISTORIC)
JADEFLAGS+=	-iinclude.historic
.else
JADEFLAGS+=	-ino.include.historic
.endif

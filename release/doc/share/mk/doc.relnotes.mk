# $FreeBSD$

DOC_PREFIX?= ${RELN_ROOT}/../../../doc

# Find the RELNOTESng document catalogs
EXTRA_CATALOGS+= ${RELN_ROOT}/${LANGCODE}/share/sgml/catalog
EXTRA_CATALOGS+= ${RELN_ROOT}/share/sgml/catalog

# Use the appropriate architecture-dependent RELNOTESng stylesheet
DSLHTML?=	${RELN_ROOT}/share/sgml/default.dsl
DSLPRINT?=	${RELN_ROOT}/share/sgml/default.dsl

#
# Automatic device list generation:
#
.if exists(${RELN_ROOT}/../man4)
MAN4DIR?=	${RELN_ROOT}/../man4
.elif exists(${RELN_ROOT}/../../man4)
MAN4DIR?=	${RELN_ROOT}/../../man4
.else
MAN4DIR?=	${RELN_ROOT}/../../share/man/man4
.endif
MAN4PAGES?=	${MAN4DIR}/*.4 ${MAN4DIR}/man4.*/*.4
ARCHLIST?=	${RELN_ROOT}/share/misc/dev.archlist.txt
DEV-AUTODIR=	${RELN_ROOT:S/${.CURDIR}/${.OBJDIR}/}/share/sgml
CLEANFILES+=	${DEV-AUTODIR}/dev-auto.sgml ${DEV-AUTODIR}/catalog-auto

MAN2HWNOTES_CMD=${RELN_ROOT}/share/misc/man2hwnotes.pl

# Dependency that the article makefiles can use to pull in
# dev-auto.sgml.
${DEV-AUTODIR}/catalog-auto ${DEV-AUTODIR}/dev-auto.sgml: ${MAN4PAGES} \
	${ARCHLIST} ${MAN2HWNOTES_CMD}
	cd ${RELN_ROOT}/share/sgml && make dev-auto.sgml

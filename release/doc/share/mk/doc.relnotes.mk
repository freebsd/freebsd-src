# $FreeBSD$

DOC_PREFIX?= ${RELN_ROOT}/../../../doc

# XXX
RELEASETYPE!= grep -o 'release.type "[a-z]*"' ${RELN_ROOT}/share/xml/release.ent | sed 's|[a-z.]* "\([a-z]*\)"|\1|'
RELEASEURL!= grep -o 'release.url \"[^\"]*\"' ${RELN_ROOT}/share/xml/release.ent | sed 's|[^ ]* "\([^"]*\)"|\1|'
RELEASEBRANCH!= grep -o 'release.branch "\([^"]*\)"' ${RELN_ROOT}/share/xml/release.ent | sed 's|[^ ]* "\([^"]*\)"|\1|'
RELEASEMAILLIST!= grep -o 'release.maillist "\([^"]*\)"' ${RELN_ROOT}/share/xml/release.ent | sed 's|[^ ]* "\([^"]*\)"|\1|'
.if ${RELEASETYPE} == "current"
PROFILING+= --param profile.attribute "'releasetype'" --param profile.value "'current'"
.elif ${RELEASETYPE} == "snapshot"
PROFILING+= --param profile.attribute "'releasetype'" --param profile.value "'snapshot'"
.elif ${RELEASETYPE} == "release"
PROFILING+= --param profile.attribute "'releasetype'" --param profile.value "'release'"
.endif
XSLTPROCFLAGS+= --param release.url "'${RELEASEURL}'"
XSLTPROCFLAGS+= --param release.branch "'${RELEASEBRANCH}'"
XSLTPROCFLAGS+= --param release.maillist "'${RELEASEMAILLIST}'"

# Find the RELNOTESng document catalogs
EXTRA_CATALOGS+= file://${RELN_ROOT}/${LANGCODE}/share/xml/catalog.xml \
		 file://${RELN_ROOT}/share/xml/catalog.xml

XSLXHTML= http://www.FreeBSD.org/release/XML/share/xml/release.xsl

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
DEV-AUTODIR=	${RELN_ROOT:S/${.CURDIR}/${.OBJDIR}/}/share/xml
CLEANFILES+=	${DEV-AUTODIR}/dev-auto.ent

MAN2HWNOTES_CMD=${RELN_ROOT}/share/misc/man2hwnotes.pl
.if defined(HWNOTES_MI)
MAN2HWNOTES_FLAGS=
.else
MAN2HWNOTES_FLAGS=	-c
.endif

# Dependency that the article makefiles can use to pull in
# dev-auto.ent.
${DEV-AUTODIR}/catalog-auto ${DEV-AUTODIR}/dev-auto.ent: ${MAN4PAGES} \
	${ARCHLIST} ${MAN2HWNOTES_CMD}
	cd ${RELN_ROOT}/share/xml && make MAN2HWNOTES_FLAGS=${MAN2HWNOTES_FLAGS} dev-auto.ent

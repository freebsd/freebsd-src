# $FreeBSD$
# Original revision: 1.3

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/early-adopter
.else
DESTDIR?= ${DOCDIR}/ja_JP.eucJP/early-adopter
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?=gz
INSTALL_ONLY_COMPRESSED?=

SRCS+= article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

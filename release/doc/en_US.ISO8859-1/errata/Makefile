# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../..
DESTDIR?= ${DOCDIR}/en_US.ISO8859-1/errata

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/early-adopter/Makefile,v 1.1 2002/11/02 14:15:25 gioria Exp $
#      Original revision: 1.3

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/early-adopter
.else
DESTDIR?= ${DOCDIR}/en_US.ISO8859-1/early-adopter
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?=gz
INSTALL_ONLY_COMPRESSED?=

SRCS+= article.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

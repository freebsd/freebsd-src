# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../..
DESTDIR?= ${DOCDIR}/en_US.ISO8859-1/readme

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# 
# SRCS lists the individual SGML files that make up the document. Changes
# to any of these files will force a rebuild
#

# SGML content
SRCS+=	article.sgml
SRCS+=	artheader.sgml
SRCS+=	abstract.sgml
SRCS+=	intro.sgml
SRCS+=	about.sgml
SRCS+=	obtaining.sgml
SRCS+=	problems.sgml
SRCS+=	ack.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

# $FreeBSD$

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=
EXTRA_CATALOGS+= ../../share/sgml/catalog

# 
# SRCS lists the individual SGML files that make up the document. Changes
# to any of these files will force a rebuild
#

# SGML content
SRCS+=	article.sgml
SRCS+=	artheader.sgml
SRCS+=	intro.sgml
SRCS+=	about.sgml
SRCS+=	obtaining.sgml
SRCS+=	problems.sgml
SRCS+=	ack.sgml

DOC_PREFIX?= /usr/doc
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

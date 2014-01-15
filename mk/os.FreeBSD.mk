# $Id: os.FreeBSD.mk 2569 2012-09-04 16:34:04Z jkoshy $
#
# Build definitions for FreeBSD

MKDOC?=		yes	# Build documentation.
MKTESTS?=	yes	# Enable the test suites.
MKTEX?=		yes	# TeX and friends are packaged in the teTeX package.
MKNOWEB?=	no	# Build literate programs.

# Link programs statically by default.
NO_SHARED?=	yes

.if defined(MKTEX) && ${MKTEX} == "yes"
EPSTOPDF?=	/usr/local/bin/epstopdf
MAKEINDEX?=	/usr/local/bin/makeindex
MPOSTTEX?=	/usr/local/bin/latex
MPOST?=		/usr/local/bin/mpost
PDFLATEX?=	/usr/local/bin/pdflatex
.endif

# Translate the spelling of a build knob (see ticket #316).
.if defined(NOMAN)
NO_MAN=		yes
.endif

# Literate programming utility.
NOWEB?=		/usr/local/bin/noweb

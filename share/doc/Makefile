#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.9 1996/09/08 21:24:15 jfieber Exp $

SUBDIR=	FAQ handbook psd smm usd papers

# List of all language-specific subdirs.
LANGSUBDIR=	ja_JP.EUC

# If ALLLANG is defined, descend to all language-specific subdirs too.
# If ALLLANG is not defined, but LANG is defined and a subdirectory with
# that name exists, descend to that directory too.
# In either case, the default subdirectories are always traversed.

.if defined(ALLLANG)
SUBDIR+=	${LANGSUBDIR}
.elif defined(LANG)
.if exists(${.CURDIR}/${LANG})
SUBDIR+=	${LANG}
.endif
.endif

# Default output formats are ascii for troff documents, and 
# ascii and html for sgml documents.  
# To specify generate postscript versions of troff documents, use: 
#  make PRINTER=ps

.include <bsd.subdir.mk>

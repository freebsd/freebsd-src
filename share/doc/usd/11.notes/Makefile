#	from: @(#)Makefile	6.1 (Berkeley) 5/26/86
#	Makefile,v 1.2 1993/07/31 15:30:23 mycroft Exp
#
#	Makefile for the notesfile reference manual
#	March 29, 1982
#	Ray Essick, University of Illinois
#	uiucdcs!essick     uiucdcs!notes
#
#	Note that the macros are set up for two-sided copying.
#	The page numbers alternate between the left and right 
#	sides of the page.
#	See the file "macros" and the macro "hh" to modify this
#
#	This copy of everything has been modified to run with troff
#	and a typesetter. Hopefully the only changes to make it
#	run again under nroff will be in page length/width statements.

VOLUME=	usd
DOC=	11.notes

SRCS=	macros 0.long \
	1 2.1 2.2 3.1 3.2 4.0 4.1 4.2 4.3 4.4 4.5 4.6 4.7 \
	5 a.checklist b.interface c.changes vtoc acks

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}

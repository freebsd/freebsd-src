# $Id: Makefile,v 1.1 1993/11/06 00:07:42 wollman Exp $
#
# Makefile for /sys/i386/doc
# This creates options.info and options.doc from options.texi, if the
# GNU makeinfo program is present, and fails miserably otherwise.
#

all:	options.info options.doc

options.info:	options.texi
	makeinfo options.texi

options.doc:	options.texi
	makeinfo -o options.doc+ --no-headers options.texi
	sed '/^General Index/,$$d' < options.doc+ > options.doc
	rm -f options.doc+

clean:
	rm -f options.info options.doc options.doc+

#
# Copyright (c) 1986, 1993 Regents of the University of California.
# All rights reserved.
#
#	From: @(#)Makefile	8.2 (Berkeley) 1/29/94
#	$FreeBSD$
#
SRCS=	man1 man2 man3 man4 man5 man6 man7 man8

MACROS=	-ms
PRINTER=ps
TBL=	tbl
TROFF=	groff

all: title.urm.${PRINTER} title.prm.${PRINTER}

print: title.urm.${PRINTER} title.prm.${PRINTER}
	lpr -${PRINTER} title.urm.${PRINTER}
	lpr -${PRINTER} title.prm.${PRINTER}

title.urm.${PRINTER}: title.urm ptxx
	${TBL} title.urm | ${TROFF} ${MACROS} >title.urm.${PRINTER}

title.prm.${PRINTER}: ${SRCS} title.prm ptxx
	${TROFF} title.prm >title.prm.${PRINTER}

ptxx:
	./tocrc

man1pages: man1
	rm -f man1.out
	for i in `cat man1`; do \
		sed -f groff.sed $$i | groff -man >>man1pages; \
	done;

man2pages: man2
	rm -f man2.out
	for i in `cat man2`; do groff -man $$i >>man2pages; done;

man3pages: man3
	rm -f man3.out
	for i in `cat man3`; do groff -man $$i >>man3pages; done;

man4pages: man4
	rm -f man4.out
	for i in `cat man4`; do groff -man $$i >>man4pages; done;

man5pages: man5
	rm -f man5.out
	for i in `cat man5`; do \
		sed -f groff.sed $$i | groff -man >>man5pages; \
	done;

man6pages: man6
	rm -f man6.out
	for i in `cat man6`; do groff -man $$i >>man6pages; done;

man7pages: man7
	rm -f man7.out
	for i in `cat man7`; do \
		sed -f groff.sed $$i | groff -man >>man7pages; \
	done;

man8pages: man8
	rm -f man8.out
	for i in `cat man8`; do groff -man $$i >>man8pages; done;

clean:
	rm -f title.urm.${PRINTER} title.prm.${PRINTER} toc? tocx? ptxx \
		*.spell errs Errs make.out

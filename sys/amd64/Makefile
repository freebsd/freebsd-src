# $FreeBSD: src/sys/amd64/Makefile,v 1.11.32.2 2008/12/04 13:32:25 kensmith Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/11/93

# Makefile for amd64 links, tags file

# SYS is normally set in Make.tags.inc
SYS=/sys

TAGDIR=	amd64

.include "../kern/Make.tags.inc"

all:
	@echo "make links or tags only"

# Directories in which to place amd64 tags links
DAMD64=	acpica amd64 ia32 include isa linux32 pci

links::
	-for i in ${COMMDIR1}; do \
	    (cd $$i && { rm -f tags; ln -s ../${TAGDIR}/tags tags; }) done
	-for i in ${COMMDIR2}; do \
	    (cd $$i && { rm -f tags; ln -s ../../${TAGDIR}/tags tags; }) done
	-for i in ${DAMD64}; do \
	    (cd $$i && { rm -f tags; ln -s ../tags tags; }) done

SAMD64=	${SYS}/amd64/acpica/*.[ch] \
	${SYS}/amd64/amd64/*.[ch] ${SYS}/amd64/ia32/*.[ch] \
	${SYS}/amd64/include/*.[ch] ${SYS}/amd64/isa/*.[ch] \
	${SYS}/amd64/linux32/*.[ch] ${SYS}/amd64/pci/*.[ch]
AAMD64=	${SYS}/amd64/amd64/*.S

tags::
	-ctags -wdt ${COMM} ${SAMD64}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAMD64} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o tags tags
	chmod 444 tags

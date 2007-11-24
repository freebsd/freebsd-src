# $FreeBSD$

# Makefile for alpha tags file

# SYS is normally set in Make.tags.inc
# SYS=/sys

TAGDIR=	alpha

.include "../kern/Make.tags.inc"

all:
	@echo "make tags only"

SALPHA=	${SYS}/alpha/mcbus/*.[ch] ${SYS}/alpha/tlsb/*.[ch] \
	${SYS}/alpha/alpha/*.[ch] ${SYS}/alpha/pci/*.[ch] \
	${SYS}/alpha/include/*.[ch] ${SYS}/alpha/isa/*.[ch] \
	${SYS}/alpha/tc/*.[ch] ${SYS}/alpha/osf1/*.[ch] \
	${SYS}/alpha/linux/*.[ch]
AALPHA=	${SYS}/alpha/alpha/*.s

tags::
	@rm -f tags
	-ctags -wdt ${COMM} ${SALPHA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AALPHA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o tags tags
	chmod 444 tags

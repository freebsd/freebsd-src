#
# $FreeBSD: src/release/picobsd/doc/src/Makefile,v 1.2.2.1 1999/08/29 15:52:42 peter Exp $
#

.include "../../Version"

DATE!="date"

DOCS= 	bugs.html faq.html hardware.html how2build.html \
	intrinsics.html intro.html TODO.html

all: ../../Version
	for i in ${DOCS}; \
	do \
		cat $${i}|sed	-e 's/@VER@/${VER}/g' \
				-e 's/@DATE@/${DATE}/g' >../$${i}; \
	done

#
# $FreeBSD: src/release/picobsd/doc/src/Makefile,v 1.4 1999/10/08 05:20:06 dwhite Exp $
#

.include "../../Version"

DATE!="date"

DOCS= 	bugs.html faq.html hardware.html how2build.html \
	intrinsics.html intro.html TODO.html installflp.html

all: ../../Version
	for i in ${DOCS}; \
	do \
		cat $${i}|sed	-e 's/@VER@/${VER}/g' \
				-e 's/@DATE@/${DATE}/g' >../$${i}; \
	done

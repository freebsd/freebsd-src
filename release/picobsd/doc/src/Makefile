#
# $FreeBSD$
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

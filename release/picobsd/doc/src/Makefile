#
#	$Id$
#

.include "../../Version"

DOCS= 	bugs.html faq.html hardware.html how2build.html \
	intrinsics.html intro.html

all: ../../Version
	for i in `ls *.html`; \
	do \
		cat $${i}|sed -e 's/@VER@/${VER}/g' >../$${i}; \
	done

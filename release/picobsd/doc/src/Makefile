#
#	$Id: Makefile,v 1.1 1998/09/26 17:23:01 abial Exp $
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

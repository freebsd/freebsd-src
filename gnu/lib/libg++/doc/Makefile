# $Id: Makefile,v 1.1 1996/10/04 08:43:37 peter Exp $

INFO = g++FAQ libg++ iostream
INFOSECTION= "Gcc Documentation"
INFOENTRY_g++FAQ= "* G++FAQ: (g++FAQ).		The GNU G++ FAQ."

.PATH: ${.CURDIR}/../../../../contrib/libg++/libg++
.PATH: ${.CURDIR}/../../../../contrib/libg++/libio

# HACK ALERT!
CLEANFILES+= lgpl.texinfo
libg++.info:	lgpl.texinfo
lgpl.texinfo:
	echo "See the file: /usr/src/gnu/COPYING.LIB" > lgpl.texinfo

.include <bsd.info.mk>

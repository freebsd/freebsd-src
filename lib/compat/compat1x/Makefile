# $Id$

DISTRIBUTION=	compat1x

LIBS=	libc.so.1.1 libcurses.so.1.1 libf2c.so.1.1 libg++.so.1.1 \
	libgcc.so.1.1 libgnumalloc.so.1.1 libgnuregex.so.1.1 libln.so.1.1 \
	libm.so.1.1 libmalloc.so.1.1 libreadline.so.1.1 libresolv.so.1.1 \
	librpcsvc.so.1.1 libskey.so.1.1 libtelnet.so.1.1 libtermcap.so.1.1 \
	libtermlib.so.1.1 libutil.so.1.1 liby.so.1.1

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS:S,libtermlib.so.1.1,,}
${lib}:	${lib}.gz.uu
	uudecode ${.CURDIR}/${lib}.gz.uu
	gunzip ${lib}.gz
.endfor

libtermlib.so.1.1: libtermcap.so.1.1
	ln libtermcap.so.1.1 libtermlib.so.1.1

install:
	${INSTALL} -c -o ${BINMODE} -g ${BINGRP} -m ${BINMODE} ${LIBS} \
		${DESTDIR}${BINDIR}

# Get all the fruit, even though we don't set PROG
.include <bsd.prog.mk>

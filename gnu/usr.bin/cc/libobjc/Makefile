LIB=	objc
CC=	gcc

NOPROFILE=no

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../lib

SRCS = hash.c sarray.c class.c sendmsg.c init.c archive.c \
	selector.c objects.c misc.c Object.m Protocol.m

OBJC_HDRS = hash.h list.h sarray.h objc.h \
         objc-api.h \
	 Object.h Protocol.h \
	 typedstream.h

OBJC_HDR_DESTDIR = ${DESTDIR}/usr/include/objc

beforeinstall:
	@-if [ ! -d ${OBJC_HDR_DESTDIR} ]; then \
		mkdir ${OBJC_HDR_DESTDIR}; \
		chown ${BINOWN}.${BINGRP} ${OBJC_HDR_DESTDIR}; \
		chmod 755 ${OBJC_HDR_DESTDIR}; \
	fi
	@echo installing includes into objc
	@-for file in ${OBJC_HDRS}; do \
		cmp -s ${.CURDIR}/$$file ${OBJC_HDR_DESTDIR}/$$file || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/$$file \
			${OBJC_HDR_DESTDIR}/$$file; \
	done

.include <bsd.lib.mk>

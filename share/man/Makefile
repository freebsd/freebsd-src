#	@(#)Makefile	8.2 (Berkeley) 4/16/94

SUBDIR=	man1 man3 man3f man4 man5 man7 man8
SEDF= /usr/share/man/makewhatis.sed

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 makewhatis.sed \
	    ${DESTDIR}${SEDF}

makedb:
	find /usr/share/man -type f -name '*.0' -print | \
	while read file; \
	do \
		sed -n -f ${SEDF} $$file; \
	done | col -b | sort -u > /tmp/whatis.db
	install -o ${BINOWN} -g ${BINGRP} -m 444 /tmp/whatis.db \
	    ${DESTDIR}/usr/share/man

.include <bsd.subdir.mk>

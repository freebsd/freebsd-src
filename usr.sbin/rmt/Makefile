#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	rmt
MAN=	rmt.8

# called from /usr/src/etc/Makefile
etc-rmt:
	rm -f ${DESTDIR}/etc/rmt
	${INSTALL_RSYMLINK} -T "package=utilities" \
	    ..${BINDIR}/rmt ${DESTDIR}/etc/rmt

.include <bsd.prog.mk>

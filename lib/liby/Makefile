#	$Id: Makefile,v 1.3 1996/11/20 20:55:11 wollman Exp $

LIB=		y
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	main.c yyerror.c

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that `ld' will not continue to generatebinaries linked
# shared against liby, so that in a future release we can move this
# off to a compat dist (like compat22).
#
beforeinstall:
	rm -f ${DESTDIR}${ORIG_SHLIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}

.include <bsd.lib.mk>

# This must follow the .include in case SHLIBDIR is defined there.
ORIG_SHLIBDIR:=	${SHLIBDIR}

# The ldconfig line in/etc/rc doesn't depend on ${LIBDIR} or ${SHLIBDIR},
# so neither does this.
SHLIBDIR=	/usr/lib/compat

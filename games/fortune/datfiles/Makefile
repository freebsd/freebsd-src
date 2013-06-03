#	@(#)Makefile	8.2 (Berkeley) 4/19/94
# $FreeBSD$

DB=	fortunes freebsd-tips murphy startrek zippy

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, COMMENT OUT THE
# NEXT LINE.
DB+=	limerick murphy-o gerrold.limerick

BLDS=	${DB:S/$/.dat/}
FILES=	${DB} ${BLDS}
CLEANFILES+=${BLDS}

FILESDIR=	${SHAREDIR}/games/fortune

.for f in ${DB}
$f.dat: $f
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Cs ${.ALLSRC} ${.TARGET}
.endfor

.include <bsd.prog.mk>

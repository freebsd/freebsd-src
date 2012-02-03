#	@(#)Makefile	8.2 (Berkeley) 4/19/94
# $FreeBSD$

FILES=	fortunes freebsd-tips murphy startrek zippy
BLDS=	fortunes.dat murphy.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat freebsd-tips.dat

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, COMMENT OUT THE
# THREE LINES AND UNCOMMENT THE FOURTH LINE.

# THE THREE LINES:
FILES+=	limerick murphy-o gerrold.limerick
BLDS+=	limerick.dat murphy-o.dat gerrold.limerick.dat
TYPE=	real

# THE FOURTH LINE:
#TYPE=	fake

FILES+=	${BLDS}
CLEANFILES+=${BLDS}

FILESDIR=	${SHAREDIR}/games/fortune

.for f in fortunes freebsd-tips gerrold.limerick limerick murphy murphy-o startrek zippy
$f.dat: $f
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Cs ${.ALLSRC} ${.TARGET}
.endfor

fortunes-o.dat: fortunes-o
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Csx ${.ALLSRC} ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	LC_ALL=C tr a-zA-Z n-za-mN-ZA-M < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>

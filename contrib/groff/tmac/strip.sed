# $FreeBSD: src/contrib/groff/tmac/strip.sed,v 1.1.1.1.4.1 2000/03/09 20:12:48 asmodai Exp $
/%beginstrip%/,$s/[	 ]*\\".*//
/^\.$/d
/%comment%/s/%comment%/.\\"/

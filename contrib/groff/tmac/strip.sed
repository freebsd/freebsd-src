# $FreeBSD: src/contrib/groff/tmac/strip.sed,v 1.2 2000/01/27 17:56:41 joerg Exp $
/%beginstrip%/,$s/[	 ]*\\".*//
/^\.$/d
/%comment%/s/%comment%/.\\"/

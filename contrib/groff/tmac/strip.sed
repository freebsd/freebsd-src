# $FreeBSD: src/contrib/groff/tmac/strip.sed,v 1.2.2.1 2000/12/07 09:48:56 ru Exp $
/%beginstrip%/,$s/[	 ]*\\".*//
/^\.$/d

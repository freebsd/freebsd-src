# $FreeBSD: src/contrib/groff/tmac/strip.sed,v 1.2.2.2 2001/04/26 17:35:41 ru Exp $
# strip comments, spaces, etc. after a line containing `%beginstrip%'
/%beginstrip%/,$ {
  s/^\.[	 ]*/./
  s/^\.\\".*/./
  s/\\".*/\\"/
  /\(.[ad]s\)/!s/[	 ]*\\"//
  /\(.[ad]s\)/s/\([^	 ]*\)\\"/\1/
  s/\([^/]\)doc-/\1/g
}
/^\.$/d

# $FreeBSD: src/contrib/groff/tmac/strip.sed,v 1.4 2001/04/17 12:28:00 ru Exp $
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

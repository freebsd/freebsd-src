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

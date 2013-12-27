#! /bin/sh
#
# Fix a problem with HTML output produced by makeinfo 4.8.
#
# groff.texinfo uses (after macro expansion) something like
#
#   @deffn ...
#   @XXindex ...
#   @deffnx ...
#
# which has worked with earlier versions (using an undocumented feature
# of the implementation of @deffn and @deffnx).  Version 4.8 has new
# code for generating HTML, and the above construction produces wrong
# HTML output: It starts a new <blockquote> without closing it properly.
# The very problem is that, according to the documentation, the @deffnx
# must immediately follow the @deffn line, making it impossible to add
# entries into user-defined indices if supplied with macro wrappers around
# @deffn and @deffnx.
#
# Note that this script is a quick hack and tightly bound to the current
# groff.texinfo macro code.  Hopefully, a new texinfo version makes it
# unnecessary.

t=${TMPDIR-.}/gro$$.tmp

cat $1 | sed '
1 {
  N
  N
}
:b
$b
N
/^<blockquote>\n<p>.*\n\n   \&mdash;/ {
  s/^<blockquote>\n<p>\(.*\n\)\n   \&mdash;/\1\&mdash;/
  n
  N
  N
  bb
}
$b
P
D
' > $t
rm $1
mv $t $1

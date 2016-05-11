#!/usr/bin/awk
#
# Copyright (c) 2003 Peter Stuge <stuge-mdoc2man@cdy.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Dramatically overhauled by Tim Kientzle.  This version almost
# handles library-style pages with Fn, Ft, etc commands.  Still
# a lot of problems...

BEGIN {
  displaylines = 0
  trailer = ""
  out = ""
  sep = ""
  nextsep = " "
}

# Add a word with appropriate preceding whitespace
# Maintain a short queue of the expected upcoming word separators.
function add(str) {
  out=out sep str
  sep = nextsep
  nextsep = " "
}

# Add a word with no following whitespace
# Use for opening punctuation such as '('
function addopen(str) {
  add(str)
  sep = ""
}

# Add a word with no preceding whitespace
# Use for closing punctuation such as ')' or '.'
function addclose(str) {
  sep = ""
  add(str)
}

# Add a word with no space before or after
# Use for separating punctuation such as '='
function addpunct(str) {
  sep = ""
  add(str)
  sep = ""
}

# Emit the current line so far
function endline() {
  addclose(trailer)
  trailer = ""
  if(length(out) > 0) {
    print out
    out=""
  }
  if(displaylines > 0) {
    displaylines = displaylines - 1
    if (displaylines == 0)
      dispend()
  }
  # First word on next line has no preceding whitespace
  sep = ""
}

function linecmd(cmd) {
  endline()
  add(cmd)
  endline()
}

function breakline() {
  linecmd(".br")
}

# Start an indented display
function dispstart() {
  linecmd(".RS 4")
}

# End an indented display
function dispend() {
  linecmd(".RE")
}

# Collect rest of input line
function wtail() {
  retval=""
  while(w<nwords) {
    if(length(retval))
      retval=retval " "
    retval=retval words[++w]
  }
  return retval
}

function splitwords(l, dest, n, o, w) {
  n = 1
  delete dest
  while (length(l) > 0) {
    sub("^[ \t]*", "", l)
    if (match(l, "^\"")) {
      l = substr(l, 2)
      o = index(l, "\"")
      if (o > 0) {
	w = substr(l, 1, o-1)
	l = substr(l, o+1)
	dest[n++] = w
      } else {
	dest[n++] = l
	l = ""
      }
    } else {
      o = match(l, "[ \t]")
      if (o > 0) {
	w = substr(l, 1, o-1)
	l = substr(l, o+1)
	dest[n++] = w
      } else {
	dest[n++] = l
	l = ""
      }
    }
  }
  return n-1
}

! /^\./ {
  out = $0
  endline()
  next
}

/^\.\\"/ { next }

{
  sub("^\\.","")
  nwords=splitwords($0, words)
  # TODO: Instead of iterating 'w' over the array, have a separate
  # function that returns 'next word' and use that.  This will allow
  # proper handling of double-quoted arguments as well.
  for(w=1;w<=nwords;w++) {
    if(match(words[w],"^Li$")) { # Literal; rest of line is unformatted
      dispstart()
      displaylines = 1
    } else if(match(words[w],"^Dl$")) { # Display literal
      dispstart()
      displaylines = 1
    } else if(match(words[w],"^Bd$")) { # Begin display
      if(match(words[w+1],"-literal")) {
        dispstart()
	linecmd(".nf")
	displaylines=10000
	w=nwords
      }
    } else if(match(words[w],"^Ed$")) { # End display
      displaylines = 0
      dispend()
    } else if(match(words[w],"^Ns$")) { # Suppress space after next word
      nextsep = ""
    } else if(match(words[w],"^No$")) { # Normal text
      add(words[++w])
    } else if(match(words[w],"^Dq$")) { # Quote
      addopen("``")
      add(words[++w])
      while(w<nwords&&!match(words[w+1],"^[\\.,]"))
	add(words[++w])
      addclose("''")
    } else if(match(words[w],"^Do$")) {
      addopen("``")
    } else if(match(words[w],"^Dc$")) {
      addclose("''")
    } else if(match(words[w],"^Oo$")) {
      addopen("[")
    } else if(match(words[w],"^Oc$")) {
      addclose("]")
    } else if(match(words[w],"^Ao$")) {
      addopen("<")
    } else if(match(words[w],"^Ac$")) {
      addclose(">")
    } else if(match(words[w],"^Dd$")) {
      date=wtail()
      next
    } else if(match(words[w],"^Dt$")) {
      id=wtail()
      next
    } else if(match(words[w],"^Ox$")) {
      add("OpenBSD")
    } else if(match(words[w],"^Fx$")) {
      add("FreeBSD")
    } else if(match(words[w],"^Nx$")) {
      add("NetBSD")
    } else if(match(words[w],"^St$")) {
      if (match(words[w+1], "^-p1003.1$")) {
         w++
         add("IEEE Std 1003.1 (``POSIX.1'')")
      } else if(match(words[w+1], "^-p1003.1-96$")) {
         w++
         add("ISO/IEC 9945-1:1996 (``POSIX.1'')")
      } else if(match(words[w+1], "^-p1003.1-88$")) {
         w++
         add("IEEE Std 1003.1-1988 (``POSIX.1'')")
      } else if(match(words[w+1], "^-p1003.1-2001$")) {
         w++
         add("IEEE Std 1003.1-2001 (``POSIX.1'')")
      } else if(match(words[w+1], "^-susv2$")) {
         w++
         add("Version 2 of the Single UNIX Specification (``SUSv2'')")
      }
    } else if(match(words[w],"^Ex$")) {
      if (match(words[w+1], "^-std$")) {
         w++
         add("The \\fB" name "\\fP utility exits 0 on success, and >0 if an error occurs.")
      }
    } else if(match(words[w],"^Os$")) {
      add(".TH " id " \"" date "\" \"" wtail() "\"")
    } else if(match(words[w],"^Sh$")) {
      section=wtail()
      add(".SH " section)
      linecmd(".ad l")
    } else if(match(words[w],"^Xr$")) {
      add("\\fB" words[++w] "\\fP(" words[++w] ")" words[++w])
    } else if(match(words[w],"^Nm$")) {
      if(match(section,"SYNOPSIS"))
        breakline()
      if(w >= nwords)
	n=name
      else if (match(words[w+1], "^[A-Z][a-z]$"))
	n=name
      else if (match(words[w+1], "^[.,;:]$"))
	n=name
      else {
        n=words[++w]
        if(!length(name))
          name=n
      }
      if(!length(n))
        n=name
      add("\\fB\\%" n "\\fP")
    } else if(match(words[w],"^Nd$")) {
      add("\\- " wtail())
    } else if(match(words[w],"^Fl$")) {
      add("\\fB\\-" words[++w] "\\fP")
    } else if(match(words[w],"^Ar$")) {
      addopen("\\fI")
      if(w==nwords)
	add("file ...\\fP")
      else
	add(words[++w] "\\fP")
    } else if(match(words[w],"^Cm$")) {
      add("\\fB" words[++w] "\\fP")
    } else if(match(words[w],"^Op$")) {
      addopen("[")
      option=1
      trailer="]" trailer
    } else if(match(words[w],"^Pp$")) {
      linecmd(".PP")
    } else if(match(words[w],"^An$")) {
      endline()
    } else if(match(words[w],"^Ss$")) {
      add(".SS")
    } else if(match(words[w],"^Ft$")) {
      if (match(section, "SYNOPSIS")) {
	breakline()
      }
      add("\\fI" wtail() "\\fP")
      if (match(section, "SYNOPSIS")) {
	breakline()
      }
    } else if(match(words[w],"^Fn$")) {
      ++w
      F = "\\fB\\%" words[w] "\\fP("
      Fsep = ""
      while(w<nwords) {
	++w
	if (match(words[w], "^[.,:]$")) {
	  --w
	  break
	}
	gsub(" ", "\\ ", words[w])
	F = F Fsep "\\fI\\%"  words[w] "\\fP"
	Fsep = ", "
      }
      add(F ")")
      if (match(section, "SYNOPSIS")) {
	addclose(";")
      }
    } else if(match(words[w],"^Fo$")) {
      w++
      F = "\\fB\\%" words[w] "\\fP("
      Fsep = ""
    } else if(match(words[w],"^Fa$")) {
      w++
      gsub(" ", "\\ ", words[w])
      F = F Fsep "\\fI\\%"  words[w] "\\fP"
      Fsep = ", "
    } else if(match(words[w],"^Fc$")) {
      add(F ")")
      if (match(section, "SYNOPSIS")) {
	addclose(";")
      }
    } else if(match(words[w],"^Va$")) {
      w++
      add("\\fI" words[w] "\\fP")
    } else if(match(words[w],"^In$")) {
      w++
      add("\\fB#include <" words[w] ">\\fP")
    } else if(match(words[w],"^Pa$")) {
      addopen("\\fI")
      w++
      if(match(words[w],"^\\."))
	add("\\&")
      add(words[w] "\\fP")
    } else if(match(words[w],"^Dv$")) {
      add(".BR")
    } else if(match(words[w],"^Em|Ev$")) {
      add(".IR")
    } else if(match(words[w],"^Pq$")) {
      addopen("(")
      trailer=")" trailer
    } else if(match(words[w],"^Aq$")) {
      addopen("\\%<")
      trailer=">" trailer
    } else if(match(words[w],"^Brq$")) {
      addopen("{")
      trailer="}" trailer
    } else if(match(words[w],"^S[xy]$")) {
      add(".B " wtail())
    } else if(match(words[w],"^Ic$")) {
      add("\\fB")
      trailer="\\fP" trailer
    } else if(match(words[w],"^Bl$")) {
      oldoptlist=optlist
      linecmd(".RS 5")
      if(match(words[w+1],"-bullet"))
	optlist=1
      else if(match(words[w+1],"-enum")) {
	optlist=2
	enum=0
      } else if(match(words[w+1],"-tag"))
	optlist=3
      else if(match(words[w+1],"-item"))
	optlist=4
      else if(match(words[w+1],"-bullet"))
	optlist=1
      w=nwords
    } else if(match(words[w],"^El$")) {
      linecmd(".RE")
      optlist=oldoptlist
    } else if(match(words[w],"^It$")&&optlist) {
      if(optlist==1)
	add(".IP \\(bu")
      else if(optlist==2)
	add(".IP " ++enum ".")
      else if(optlist==3) {
	add(".TP")
        endline()
	if(match(words[w+1],"^Pa$|^Ev$")) {
	  add(".B")
	  w++
	}
      } else if(optlist==4)
	add(".IP")
    } else if(match(words[w],"^Xo$")) {
      # TODO: Figure out how to handle this
    } else if(match(words[w],"^Xc$")) {
      # TODO: Figure out how to handle this
    } else if(match(words[w],"^[=]$")) {
      addpunct(words[w])
    } else if(match(words[w],"^[[{(]$")) {
      addopen(words[w])
    } else if(match(words[w],"^[\\])}.,;:]$")) {
      addclose(words[w])
    } else {
      add(words[w])
    }
  }
  if(match(out,"^\\.[^a-zA-Z]"))
    sub("^\\.","",out)
  endline()
}

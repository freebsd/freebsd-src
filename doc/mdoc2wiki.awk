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
  listdepth = 0
  trailer = ""
  out = ""
  sep = ""
  nextsep = " "
  spaces = "                    "

  NORMAL_STATE = 0
  PRETAG_STATE = 1
  STATE = NORMAL_STATE
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
    if (STATE == PRETAG_STATE) {
      print out
    } else {
      print out " "
    }
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
  linecmd("<br>")
}

function crossref(name, sect, other) {
  if (name == "cpio" && sect == 1) {
    n = "ManPageBsdcpio1"
  } else if (name == "cpio" && sect == 5) {
    n = "ManPageCpio5"
  } else if (name == "mtree" && sect == 5) {
    n = "ManPageMtree5"
  } else if (name == "tar" && sect == 1) {
    n = "ManPageBsdtar1"
  } else if (name == "tar" && sect == 5) {
    n = "ManPageTar5"
  } else if (!match(name, "^archive") && !match(name, "^libarchive")) {
    n = name "(" sect ")|http://www.freebsd.org/cgi/man.cgi?query=" name "&sektion=" sect
  } else {
    n = "ManPage"
    numbits = split(name, namebits, "[_-]")
    for (i = 1; i <= numbits; ++i) {
      p = namebits[i]
      n = n toupper(substr(p, 0, 1)) substr(p, 2)
    }
    n = n sect
  }
  n = "[[" n "]]"
  if (length other > 0)
    n = n other
  return n
}

# Start an indented display
function dispstart() {
  endline()
  print "```text"
}

# End an indented display
function dispend() {
  endline()
  print "```"
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
  gsub("\\\\e", "\\")
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
      STATE = PRETAG_STATE
      if(match(words[w+1],"-literal")) {
        dispstart()
        displaylines=10000
        w=nwords
      }
    } else if(match(words[w],"^Ed$")) { # End display
      displaylines = 0
      dispend()
      STATE = NORMAL_STATE
    } else if(match(words[w],"^Ns$")) { # Suppress space before next word
      sep=""
    } else if(match(words[w],"^No$")) { # Normal text
      add(words[++w])
    } else if(match(words[w],"^Dq$")) { # Quote
      addopen("\"")
      add(words[++w])
      while(w<nwords&&!match(words[w+1],"^[\\.,]"))
        add(words[++w])
      addclose("\"")
    } else if(match(words[w],"^Do$")) {
      addopen("\"")
    } else if(match(words[w],"^Dc$")) {
      addclose("\"")
    } else if(match(words[w],"^Oo$")) {
      addopen("<nowiki>[</nowiki>")
    } else if(match(words[w],"^Oc$")) {
      addclose("<nowiki>]</nowiki>")
    } else if(match(words[w],"^Ao$")) {
      addopen("&lt;")
    } else if(match(words[w],"^Ac$")) {
      addclose("&gt;")
    } else if(match(words[w],"^Dd$")) {
      date=wtail()
      next
    } else if(match(words[w],"^Dt$")) {
      id=words[++w] "(" words[++w] ")"
      next
    } else if(match(words[w],"^Ox$")) {
      add("OpenBSD")
    } else if(match(words[w],"^Fx$")) {
      add("FreeBSD")
    } else if(match(words[w],"^Bx$")) {
      add("BSD")
    } else if(match(words[w],"^Nx$")) {
      add("NetBSD")
    } else if(match(words[w],"^St$")) {
      if (match(words[w+1], "^-p1003.1$")) {
         w++
         add("<nowiki>IEEE Std 1003.1 (``POSIX.1'')</nowiki>")
      } else if(match(words[w+1], "^-p1003.1-96$")) {
         w++
         add("<nowiki>ISO/IEC 9945-1:1996 (``POSIX.1'')</nowiki>")
      } else if(match(words[w+1], "^-p1003.1-88$")) {
         w++
         add("<nowiki>IEEE Std 1003.1-1988 (``POSIX.1'')</nowiki>")
      } else if(match(words[w+1], "^-p1003.1-2001$")) {
         w++
         add("<nowiki>IEEE Std 1003.1-2001 (``POSIX.1'')</nowiki>")
      } else if(match(words[w+1], "^-susv2$")) {
         w++
         add("<nowiki>Version 2 of the Single UNIX Specification (``SUSv2'')</nowiki>")
      }
    } else if(match(words[w],"^Ex$")) {
      if (match(words[w+1], "^-std$")) {
         w++
         add("The '''" name "''' utility exits 0 on success, and &gt;0 if an error occurs.")
      }
    } else if(match(words[w],"^Os$")) {
      add(id " manual page")
    } else if(match(words[w],"^Sh$")) {
      section=wtail()
      linecmd("== " section " ==")
    } else if(match(words[w],"^Xr$")) {
      add(crossref(words[w+1], words[w+2], words[w+3]))
      w = w + 3
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
      if (displaylines == 0)
        add("'''" n "'''")
      else
        add(n)
    } else if(match(words[w],"^Nd$")) {
      add("- " wtail())
    } else if(match(words[w],"^Fl$")) {
      addopen("-")
    } else if(match(words[w],"^Ar$")) {
      if(w==nwords)
        add("''file ...''")
      else {
        ++w
        gsub("<", "\\&lt;", words[w])
        if (displaylines > 0)
          add(words[w])
        else
          add("''" words[w] "''")
      }
    } else if(match(words[w],"^Cm$")) {
      ++w
      if (displaylines == 0) {
        add("'''" words[w] "'''")
      } else
        add(words[w])
    } else if(match(words[w],"^Op$")) {
      addopen("<nowiki>[</nowiki>")
      option=1
      trailer="<nowiki>]</nowiki>" trailer
    } else if(match(words[w],"^Pp$")) {
      ++w
      endline()
      print ""
    } else if(match(words[w],"^An$")) {
      if (match(words[w+1],"-nosplit"))
        ++w
      endline()
    } else if(match(words[w],"^Ss$")) {
      add("===")
      trailer="==="
    } else if(match(words[w],"^Ft$")) {
      if (match(section, "SYNOPSIS")) {
        breakline()
      }
      l = wtail()
      add("''" l "''")
      if (match(section, "SYNOPSIS")) {
        breakline()
      }
    } else if(match(words[w],"^Fn$")) {
      ++w
      F = "'''" words[w] "'''("
      Fsep = ""
      while(w<nwords) {
        ++w
        if (match(words[w], "^[.,:]$")) {
          --w
          break
        }
        F = F Fsep "''"  words[w] "''"
        Fsep = ", "
      }
      add(F ")")
      if (match(section, "SYNOPSIS")) {
        addclose(";")
      }
    } else if(match(words[w],"^Fo$")) {
      w++
      F = "'''" words[w] "'''("
      Fsep = ""
    } else if(match(words[w],"^Fa$")) {
      w++
      F = F Fsep "''"  words[w] "''"
      Fsep = ", "
    } else if(match(words[w],"^Fc$")) {
      add(F ")")
      if (match(section, "SYNOPSIS")) {
        addclose(";")
      }
    } else if(match(words[w],"^Va$")) {
      w++
      add("''" words[w] "''")
    } else if(match(words[w],"^In$")) {
      w++
      add("'''<nowiki>#include <" words[w] "></nowiki>'''")
    } else if(match(words[w],"^Pa$")) {
      w++
#      if(match(words[w],"^\\."))
#       add("\\&")
      if (displaylines == 0)
        add("''" words[w] "''")
      else
        add(words[w])
    } else if(match(words[w],"^Dv$")) {
      linecmd()
    } else if(match(words[w],"^Em|Ev$")) {
      add(".IR")
    } else if(match(words[w],"^Pq$")) {
      addopen("(")
      trailer=")" trailer
    } else if(match(words[w],"^Aq$")) {
      addopen(" &lt;")
      trailer="&gt;" trailer
    } else if(match(words[w],"^Brq$")) {
      addopen("<nowiki>{</nowiki>")
      trailer="<nowiki>}</nowiki>" trailer
    } else if(match(words[w],"^S[xy]$")) {
      add(".B " wtail())
    } else if(match(words[w],"^Tn$")) {
      n=wtail()
      add("'''" n "'''")
    } else if(match(words[w],"^Ic$")) {
      add("''")
      trailer="''" trailer
    } else if(match(words[w],"^Bl$")) {
      ++listdepth
      listnext[listdepth]=""
      if(match(words[w+1],"-bullet")) {
        optlist[listdepth]=1
        addopen("<ul>")
        listclose[listdepth]="</ul>"
      } else if(match(words[w+1],"-enum")) {
        optlist[listdepth]=2
        enum=0
        addopen("<ol>")
        listclose[listdepth]="</ol>"
      } else if(match(words[w+1],"-tag")) {
        optlist[listdepth]=3
        addopen("<dl>")
        listclose[listdepth]="</dl>"
      } else if(match(words[w+1],"-item")) {
        optlist[listdepth]=4
        addopen("<ul>")
        listclose[listdepth]="</ul>"
      }
      w=nwords
    } else if(match(words[w],"^El$")) {
      addclose(listnext[listdepth])
      addclose(listclose[listdepth])
      listclose[listdepth]=""
      listdepth--
    } else if(match(words[w],"^It$")) {
      addclose(listnext[listdepth])
      if(optlist[listdepth]==1) {
        addpunct("<li>")
        listnext[listdepth] = "</li>"
      } else if(optlist[listdepth]==2) {
        addpunct("<li>")
        listnext[listdepth] = "</li>"
      } else if(optlist[listdepth]==3) {
        addpunct("<dt>")
        listnext[listdepth] = "</dt>"
        if(match(words[w+1],"^Xo$")) {
          # Suppress trailer
          w++
        } else if(match(words[w+1],"^Pa$|^Ev$")) {
          addopen("'''")
          w++
          add(words[++w] "'''")
          trailer = listnext[listdepth] "<dd>" trailer
          listnext[listdepth] = "</dd>"
        } else {
          trailer = listnext[listdepth] "<dd>" trailer
          listnext[listdepth] = "</dd>"
        }
      } else if(optlist[listdepth]==4) {
        addpunct("<li>")
        listnext[listdepth] = "</li>"
      }
    } else if(match(words[w], "^Vt$")) {
      w++
      add("''" words[w] "''")
    } else if(match(words[w],"^Xo$")) {
      # TODO: Figure out how to handle this
    } else if(match(words[w],"^Xc$")) {
      # TODO: Figure out how to handle this
      if (optlist[listdepth] == 3) {
        addclose(listnext[listdepth])
        addopen("<dd>")
        listnext[listdepth] = "</dd>"
      }
    } else if(match(words[w],"^[=]$")) {
      addpunct(words[w])
    } else if(match(words[w],"^[[{(]$")) {
      addopen(words[w])
    } else if(match(words[w],"^[\\])}.,;:]$")) {
      addclose(words[w])
    } else {
      sub("\\\\&", "", words[w])
      add(words[w])
    }
  }
  if(match(out,"^\\.[^a-zA-Z]"))
    sub("^\\.","",out)
  endline()
}

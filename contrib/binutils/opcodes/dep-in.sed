:loop
/\\$/N
s/\\\n */ /g
t loop

s!\.o:!.lo:!
s! @BFD_H@! $(BFD_H)!g
s!@INCDIR@!$(INCDIR)!g
s!@BFDDIR@!$(BFDDIR)!g
s!@SRCDIR@/!!g

s/\\\n */ /g

s/ *$//
s/  */ /g
s/ *:/:/g
/:$/d

s/\(.\{50\}[^ ]*\) /\1 \\\
  /g

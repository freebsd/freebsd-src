:loop
/\\$/N
/\\$/b loop

s! @BFD_H@!!g
s!@INCDIR@!$(INCDIR)!g
s!@SRCDIR@/!!g
s!hosts/[^ ]*\.h ! !g
s/ sysdep.h//g
s/ libbfd.h//g
s/ config.h//g
s! \$(INCDIR)/fopen-[^ ]*\.h!!g
s! \$(INCDIR)/ansidecl\.h!!g
s! \$(INCDIR)/obstack\.h!!g

s/\\\n */ /g

s/ *$//
s/  */ /g
s/ *:/:/g
/:$/d

s/\(.\{50\}[^ ]*\) /\1 \\\
  /g

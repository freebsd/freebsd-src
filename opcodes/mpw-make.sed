# Sed commands to finish translating the opcodes Makefile.in into MPW syntax.

# Empty HDEFINES.
/HDEFINES/s/@HDEFINES@//

# Fix pathnames to include directories.
/^INCDIR = /s/^INCDIR = .*$/INCDIR = "{topsrcdir}"include/
/^CSEARCH = /s/$/ -i "{INCDIR}":mpw: -i ::extra-include:/

/BFD_MACHINES/s/@BFD_MACHINES@/{BFD_MACHINES}/
/archdefs/s/@archdefs@/{ARCHDEFS}/

# No PIC foolery in this environment.
/@ALLLIBS@/s/@ALLLIBS@/{TARGETLIB}/
/@PICLIST@/s/@PICLIST@//
/@PICFLAG@/s/@PICFLAG@//
/^{OFILES} \\Option-f stamp-picdir/,/^$/d

# Remove the pic trickery from the default build rule.
/^\.c\.o \\Option-f /,/End If/c\
.c.o \\Option-f .c

# Remove pic trickery from other rules - aimed at the rule
# for disassemble.o in particular.
/-n "{PICFLAG}"/,/End If/d

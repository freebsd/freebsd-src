# Sed commands to finish translating the Unix BFD Makefile into MPW syntax.

# Whack out unused host and target define bits.
/HDEFINES/s/@HDEFINES@//
/TDEFINES/s/@TDEFINES@//

# Fix pathnames to include directories.
/^INCDIR = /s/^INCDIR = .*$/INCDIR = "{topsrcdir}"include/
/^CSEARCH = /s/$/ -i "{INCDIR}":mpw: -i ::extra-include:/

# Comment out setting of vars, configure script will add these itself.
/^WORDSIZE =/s/^/#/
# /^ALL_BACKENDS/s/^/#/
/^BFD_BACKENDS/s/^/#/
/^BFD_MACHINES/s/^/#/
/^TDEFAULTS/s/^/#/

# Remove extra, useless, "all".
/^all \\Option-f _oldest/,/^$/d

# Remove the Makefile rebuild rule.
/^Makefile /,/--recheck/d

# Don't do any recursive subdir stuff.
/ subdir_do/s/{MAKE}/null-command/

/BFD_H/s/^{BFD_H}/#{BFD_H}/

# Add explicit srcdir paths to special files.
/config.bfd/s/ config.bfd/ "{s}"config.bfd/g
/targmatch.sed/s/ targmatch.sed/ "{s}"targmatch.sed/g

# Point at include files that are always in the objdir.
/bfd/s/"{s}"bfd\.h/"{o}"bfd.h/g
/config/s/"{s}"config\.h/"{o}"config.h/g
/targmatch/s/"{s}"targmatch\.h/"{o}"targmatch.h/g
/targmatch/s/^targmatch\.h/"{o}"targmatch.h/
/elf32-target/s/"{s}"elf32-target\.h/"{o}"elf32-target.h/g
/elf32-target/s/^elf32-target\.h/"{o}"elf32-target.h/
/elf64-target/s/"{s}"elf64-target\.h/"{o}"elf64-target.h/g
/elf64-target/s/^elf64-target\.h/"{o}"elf64-target.h/

/"{s}"{INCDIR}/s/"{s}"{INCDIR}/"{INCDIR}"/g

/dep/s/\.dep/__dep/g

# Removing duplicates is cool but presently unnecessary,
# so whack this out.
/^ofiles \\Option-f/,/^$/d
/ofiles/s/{OFILES} ofiles/{OFILES}/
/echo ofiles = /d
/cat ofiles/s/`cat ofiles`/{OFILES}/

# No corefile support.
/COREFILE/s/@COREFILE@//
/COREFLAG/s/@COREFLAG@//

# No PIC foolery in this environment.
/@ALLLIBS@/s/@ALLLIBS@/{TARGETLIB}/
/@PICLIST@/s/@PICLIST@//
/@PICFLAG@/s/@PICFLAG@//
/^{OFILES} \\Option-f stamp-picdir/,/^$/d

# Remove the pic trickery from the default build rule.
/^\.c\.o \\Option-f /,/End If/c\
.c.o \\Option-f .c

# MPW Make doesn't know about $<.
/"{o}"targets.c.o \\Option-f "{s}"targets.c Makefile/,/^$/c\
"{o}"targets.c.o \\Option-f "{s}"targets.c Makefile\
	{CC} @DASH_C_FLAG@ {ALL_CFLAGS} {TDEFAULTS} "{s}"targets.c -o "{o}"targets.c.o

/"{o}"archures.c.o \\Option-f "{s}"archures.c Makefile/,/^$/c\
"{o}"archures.c.o \\Option-f "{s}"archures.c Makefile\
	{CC} @DASH_C_FLAG@ {ALL_CFLAGS} {TDEFAULTS} "{s}"archures.c -o "{o}"archures.c.o

# Remove the .h rebuilding rules, we don't currently have a doc subdir,
# or a way to build the prototype-hacking tool that's in it.
/^"{srcdir}"bfd-in2.h \\Option-f /,/^$/d
/^"{srcdir}"libbfd.h \\Option-f /,/^$/d
/^"{srcdir}"libcoff.h \\Option-f /,/^$/d

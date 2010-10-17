# Sed commands to finish translating the ld Makefile.in into MPW syntax.

/HDEFINES/s/@HDEFINES@//

/^target_alias = @target_alias@/s/^/#/

/^EMUL = @EMUL@/s/^/#/

/^EMULATION_OFILES = @EMULATION_OFILES@/s/^/#/

# Fixadd to the include paths.
/^INCLUDES = .*$/s/$/ -i "{INCDIR}":mpw: -i ::extra-include:/
/BFDDIR/s/-i {BFDDIR} /-i "{BFDDIR}": /
/INCDIR/s/-i {INCDIR} /-i "{INCDIR}": /

# Use byacc instead of bison (for now anyway).
/BISON/s/^BISON =.*$/BISON = byacc/
#/BISONFLAGS/s/^BISONFLAGS =.*$/BISONFLAGS = /

# Suppress the suppression of smart makes.
/^\.y\.c/d

# Hack up ldmain compile.
/^"{o}"ldmain.c.o \\Option-f .* config.status$/,/^$/c\
"{o}"ldmain.c.o \\Option-f  "{s}"ldmain.c\
	{CC} @DASH_C_FLAG@ -d DEFAULT_EMULATION={dq}{EMUL}{dq} -d SCRIPTDIR={dq}{scriptdir}{dq} {ALL_CFLAGS} "{s}"ldmain.c -o "{o}"ldmain.c.o\


# Remove ldemul-list.h build, rely on configure to make one.
/^ldemul-list.h /,/Rename -y "{s}"ldemul-tmp.h /d

# Fix pathnames to generated files.
/config.h/s/"{s}"config\.h/"{o}"config.h/g
/config.h/s/^config\.h/"{o}"config.h/

/y.tab.c/s/"{s}"y\.tab\.c/"{o}"y.tab.c/g
/y.tab.c/s/^y\.tab\.c/"{o}"y.tab.c/
/y.tab.h/s/"{s}"y\.tab\.h/"{o}"y.tab.h/g
/y.tab.h/s/^y\.tab\.h/"{o}"y.tab.h/

/ldgram.c/s/"{s}"ldgram\.c/"{o}"ldgram.c/g
/ldgram.c/s/^ldgram\.c/"{o}"ldgram.c/

/ldgram.h/s/"{s}"ldgram\.h/"{o}"ldgram.h/g
/ldgram.h/s/^ldgram\.h/"{o}"ldgram.h/

/ldlex.c/s/"{s}"ldlex\.c/"{o}"ldlex.c/g
/ldlex.c/s/^ldlex\.c/"{o}"ldlex.c/

/ldlex.c.new/s/"{s}"ldlex\.c\.new/"{o}"ldlex.c.new/g

/lex.yy.c/s/"{s}"lex\.yy\.c/"{o}"lex.yy.c/g

/ldemul-list.h/s/"{s}"ldemul-list\.h/"{o}"ldemul-list.h/g
/ldemul-list.h/s/^ldemul-list\.h/"{o}"ldemul-list.h/

# Edit pathnames to emulation files.
/"{s}"e.*\.c/s/"{s}"e\([-_a-z0-9]*\)\.c/"{o}"e\1.c/g
/^e.*\.c/s/^e\([-_a-z0-9]*\)\.c/"{o}"e\1.c/

# We can't run genscripts, so don't try.
/{GENSCRIPTS}/s/{GENSCRIPTS}/null-command/

# Comment out the TDIRS bits.
/^TDIRS@/s/^/#/

# Point at the BFD library directly.
/@BFDLIB@/s/@BFDLIB@/::bfd:libbfd.o/

# Don't need this.
/@HLDFLAGS@/s/@HLDFLAGS@//

#/sed.*free/,/> "{o}"ldlex.c.new/c\
#	\	Catenate "{o}"lex.yy.c >"{o}"ldlex.c.new

# The resource file is called mac-ld.r.
/{LD_PROG}.r/s/{LD_PROG}\.r/mac-ld.r/

/^install \\Option-f /,/^$/c\
install \\Option-f  all install-only\
\
install-only \\Option-f\
	NewFolderRecursive "{bindir}"\
	Duplicate -y :ld.new "{bindir}"ld\


# Remove dependency rebuilding crud.
/^.dep /,/# .PHONY /d

# Remove the lintlog action, pipe symbols in column 1 lose.
/^lintlog \\Option-f/,/^$/d

/^Makefile \\Option-f/,/^$/d
/^"{o}"config.h \\Option-f/,/^$/d
/^config.status \\Option-f/,/^$/d

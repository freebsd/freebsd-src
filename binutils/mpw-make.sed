# Sed commands to finish translating the binutils Unix makefile into MPW syntax.

# Add a rule.
/^#### .*/a\
\
"{o}"underscore.c.o \\Option-f "{o}"underscore.c\

# Comment out any alias settings.
/^host_alias =/s/^/#/
/^target_alias =/s/^/#/

# Whack out unused host define bits.
/HDEFINES/s/@HDEFINES@//

# Don't build specialized tools.
/BUILD_NLMCONV/s/@BUILD_NLMCONV@//
/BUILD_SRCONV/s/@BUILD_SRCONV@//
/BUILD_DLLTOOL/s/@BUILD_DLLTOOL@//

/UNDERSCORE/s/@UNDERSCORE@/{UNDERSCORE}/

# Don't need this.
/@HLDFLAGS@/s/@HLDFLAGS@//

# Point at the libraries directly.
/@BFDLIB@/s/@BFDLIB@/::bfd:libbfd.o/
/@OPCODES@/s/@OPCODES@/::opcodes:libopcodes.o/

# Whack out target makefile fragment.
/target_makefile_fragment/s/target_makefile_fragment@//

# Fix and add to the include paths.
/^INCLUDES = .*$/s/$/ -i "{INCDIR}":mpw: -i ::extra-include:/
/BFDDIR/s/-i {BFDDIR} /-i "{BFDDIR}": /
/INCDIR/s/-i {INCDIR} /-i "{INCDIR}": /

# Use byacc instead of bison (for now anyway).
/BISON/s/^BISON =.*$/BISON = byacc/
#/BISONFLAGS/s/^BISONFLAGS =.*$/BISONFLAGS = /

# Embed the version in symbolic doublequotes that will expand to
# the right thing for each compiler.
/VERSION/s/'"{VERSION}"'/{dq}{VERSION}{dq}/

# '+' is a special char to MPW, don't use it ever.
/c++filt/s/c++filt/cplusfilt/

# All of the binutils use the same Rez file, change names to refer to it.
/^{[A-Z]*_PROG}/s/$/ "{s}"mac-binutils.r/
/{[A-Z]*_PROG}\.r/s/{[A-Z]*_PROG}\.r/mac-binutils.r/

# There are auto-generated references to BFD .h files that are not
# in the objdir (like bfd.h) but are in the source dir.
/::bfd:lib/s/::bfd:lib\([a-z]*\)\.h/{BFDDIR}:lib\1.h/g

# Fix the locations of generated files.
/config/s/"{s}"config\.h/"{o}"config.h/g
/config/s/^config\.h/"{o}"config\.h/
/underscore/s/"{s}"underscore\.c/"{o}"underscore.c/g
/underscore/s/^underscore\.c/"{o}"underscore\.c/

# Fix paths to generated source files.
/lex.yy.c/s/"{s}"lex\.yy\.c/"{o}"lex.yy.c/g
/lex.yy.c/s/^lex\.yy\.c/"{o}"lex.yy.c/
/arlex.c/s/"{s}"arlex\.c/"{o}"arlex.c/g
/arlex.c/s/^arlex\.c/"{o}"arlex.c/
/y.tab.c/s/"{s}"y\.tab\.c/"{o}"y.tab.c/g
/y.tab.c/s/^y\.tab\.c/"{o}"y.tab.c/
/arparse.c/s/"{s}"arparse\.c/"{o}"arparse.c/g
/arparse.c/s/^arparse\.c/"{o}"arparse.c/
/y.tab.h/s/"{s}"y\.tab\.h/"{o}"y.tab.h/g
/y.tab.h/s/^y\.tab\.h/"{o}"y.tab.h/
/arparse.h/s/"{s}"arparse\.h/"{o}"arparse.h/g
/arparse.h/s/^arparse\.h/"{o}"arparse.h/

/"{s}"{INCDIR}/s/"{s}"{INCDIR}/"{INCDIR}"/g

# The generated lexer may include an ifdef for older Mac compilers that
# needs to work with newer compilers also.
/lex.yy.c/s/Rename -y \([^ ]*\) \([^ ]*\)$/sed -e 's,ifdef macintosh,if defined(macintosh) || defined(__MWERKS__),' \1 > \2/

# Fix an over-eagerness.
/echo.*WARNING.*This file/s/'.*'/' '/

# Add a "stamps" target.
$a\
stamps \\Option-f stamp-under\

/^install \\Option-f /,/^$/c\
install \\Option-f  all install-only\
\
install-only \\Option-f\
	NewFolderRecursive "{bindir}"\
	# Need to copy all the tools\
	For prog in {PROGS}\
		Set progname `echo {prog} | sed -e 's/.new//'`\
		Duplicate -y :{prog} "{bindir}"{progname}\
	End For\


/true/s/ ; @true$//

# dot files are trouble, remove them and their actions.
/^\.dep/,/^$/d

# Remove un-useful targets.
/^Makefile \\Option-f/,/^$/d
/^"{o}"config.h \\Option-f/,/^$/d
/^config.status \\Option-f/,/^$/d

# Don't try to make the demangler's man page, it's useless.
/^{DEMANGLER_PROG}\.1 \\Option-f/,/^$/d
# Don't depend on it either.
/{DEMANGLER_PROG}/s/ {DEMANGLER_PROG}\.1//


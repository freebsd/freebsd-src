# Sed commands that finish translating the GDB Unix Makefile to MPW syntax.

/^host_alias =/s/^/#/
/^target_alias =/s/^/#/

/^host_makefile_frag@$/d
/^target_makefile_frag@$/d

/@ENABLE_CFLAGS@/s/@ENABLE_CFLAGS@/{ENABLE_CFLAGS}/g
/^ENABLE_CFLAGS=/s/^/#/

# Edit all the symbolic definitions pointing to various libraries and such.

/^INCLUDE_DIR = /s/"{srcdir}":include/"{topsrcdir}"include:/

/^MMALLOC_DIR = /s/::mmalloc/mmalloc:/
/^MMALLOC_SRC = /s/"{srcdir}"/"{topsrcdir}"/
/^MMALLOC =/s/=.*$/=/
/MMALLOC_CFLAGS =/s/=.*$/= -u USE_MMALLOC/

/^BFD_DIR = /s/::bfd/bfd:/
/^BFD = /s/{BFD_DIR}:libbfd/{BFD_DIR}libbfd/
/^BFD_SRC = /s/"{srcdir}"/"{topsrcdir}"/

/^READLINE_DIR = /s/::readline/readline:/
/^READLINE =/s/=.*$/=/
/^READLINE_SRC = /s/"{srcdir}"/"{topsrcdir}"/

/^INCLUDE_CFLAGS = /s/$/ -i "{topsrcdir}"include:mpw: -i ::extra-include:/

/^SER_HARDWIRE =/s/ser-unix/ser-mac/

/^TERMCAP =/s/ =.*$/ =/

# Whack out various autoconf vars that we don't need.
/@CONFIG_LDFLAGS@/s/@CONFIG_LDFLAGS@//g
/@HLDFLAGS@/s/@HLDFLAGS@//g
/@DEFS@/s/@DEFS@//g
/@YACC@/s/@YACC@/byacc/g
/@ENABLE_OBS@/s/@ENABLE_OBS@//g
/@ENABLE_CLIBS@/s/@ENABLE_CLIBS@//g
/@LIBS@/s/@LIBS@//g

# Whack out autoconf hook for thread debugging.
/@THREAD_DB_OBS@/s/@THREAD_DB_OBS@//g

# Fix up paths to include directories.
/INCLUDE_DIR/s/"{s}"{INCLUDE_DIR}/{INCLUDE_DIR}/g
/INCLUDE_DIR/s/{INCLUDE_DIR}:/{INCLUDE_DIR}/g
/INCLUDE_DIR/s/"{INCLUDE_DIR}":/"{INCLUDE_DIR}"/g

/{BFD_DIR}/s/"{BFD_DIR}":/"{BFD_DIR}"/g
/{BFD_DIR}/s/\([ 	]\){BFD_DIR}/\1::{BFD_DIR}/g
/{BFD_DIR}/s/\([ 	]\)"{BFD_DIR}"/\1::"{BFD_DIR}"/g

/{BFD_SRC}/s/"{s}"{BFD_SRC}/{BFD_SRC}/g
/{BFD_SRC}/s/{BFD_SRC}:/{BFD_SRC}/g

/{READLINE_SRC}/s/"{s}"{READLINE_SRC}/{READLINE_SRC}/g

/^readline_headers =/,/^$/c\
readline_headers =\


# This isn't really useful, and seems to cause nonsensical complaints.
/{ALLDEPFILES}/s/{ALLDEPFILES}//g

/^copying.c \\Option-f /,/^$/d

# Fix the syntax of bits of C code that go into version.c.
/char /s/'char .Option-x/'char */

# Point at files in the obj dir rather than src dir.
/version/s/"{s}"version\.c/"{o}"version.c/g
/version/s/^version\.c/"{o}"version.c/
/config/s/"{s}"config\.h/"{o}"config.h/g
/config/s/^config\.h/"{o}"config.h/
/xm/s/"{s}"xm\.h/"{o}"xm.h/g
/xm/s/^xm\.h/"{o}"xm.h/
/tm/s/"{s}"tm\.h/"{o}"tm.h/g
/tm/s/^tm\.h/"{o}"tm.h/
/nm/s/"{s}"nm\.h/"{o}"nm.h/g
/nm/s/^nm\.h/"{o}"nm.h/

/exp.tab.c/s/"{s}"\([a-z0-9]*\)-exp\.tab\.c/"{o}"\1-exp.tab.c/g
/exp.tab.c/s/^\([a-z0-9]*\)-exp\.tab\.c/"{o}"\1-exp.tab.c/

/y.tab/s/"{s}"y.tab\.c/"{o}"y.tab.c/g
/y.tab/s/^y.tab\.c/"{o}"y.tab.c/

/init/s/"{s}"init\.c-tmp/"{o}"init.c-tmp/g
/init/s/^init\.c-tmp/"{o}"init.c-tmp/
/init/s/"{s}"init\.c/"{o}"init.c/g
/init/s/^init\.c/"{o}"init.c/

# Fix up the generation of version.c.
/"{o}"version.c \\Option-f Makefile/,/^$/c\
"{o}"version.c \\Option-f Makefile\
	echo -n 'char *version = "'	 >"{o}"version.c\
	echo -n "{VERSION}"		>>"{o}"version.c\
	echo '";'			>>"{o}"version.c\
	echo -n 'char *host_name = "'	>>"{o}"version.c\
	echo -n "{host_alias}"		>>"{o}"version.c\
	echo '";'			>>"{o}"version.c\
	echo -n 'char *target_name = "'	>>"{o}"version.c\
	echo -n "{target_alias}"	>>"{o}"version.c\
	echo '";'			>>"{o}"version.c\


/ansidecl/s/include "{s}""ansidecl.h"/include "ansidecl.h"/

# Open-brace in a command causes much confusion; replace with the
# result from a script.
/initialize_all_files ()/c\
	Echo -n 'void initialize_all_files () ' >> "{o}"init.c-tmp\
	open-brace >> "{o}"init.c-tmp

# Replace the whole sed bit for init.c; it's simpler that way...
/echo {OBS} {TSOBS}/,/echo '}'/c\
	For i in {OBS} {TSOBS}\
          Set filename "`Echo {i} | sed \\Option-d\
            -e '/^Onindy.c.o/d' \\Option-d\
            -e '/^nindy.c.o/d' \\Option-d\
            -e '/ttyflush.c.o/d' \\Option-d\
            -e '/xdr_ld.c.o/d' \\Option-d\
            -e '/xdr_ptrace.c.o/d' \\Option-d\
            -e '/xdr_rdb.c.o/d' \\Option-d\
            -e '/udr.c.o/d' \\Option-d\
            -e '/udip2soc.c.o/d' \\Option-d\
            -e '/udi2go32.c.o/d' \\Option-d\
            -e '/version.c.o/d' \\Option-d\
            -e '/[a-z0-9A-Z_]*-exp.tab.c.o/d' \\Option-d\
            -e 's/\\.c\\.o/.c/' \\Option-d\
            -e 's/^://'`"\
          If "{filename}" != ""\
            sed <"{s}""{filename}" >>"{o}"init.c-tmp -n \\Option-d\
            -e '/^_initialize_[a-z_0-9A-Z]* *(/s/^\\([a-z_0-9A-Z]*\\).*/  {extern void \\1 (); \\1 ();}/p'\
          End If\
	End For\
	Echo '}' >>"{o}"init.c-tmp

# Fix the main compile/link command.
/{CC_LD} {INTERNAL_LDFLAGS} -o gdb/,/"{o}"init.c.o {OBS} {TSOBS} {ADD_FILES} {CLIBS} {LOADLIBES}/c\
	{CC_LD} {INTERNAL_LDFLAGS} -o gdb{PROG_EXT} "{o}"init.c.o {OBS} {TSOBS} {ADD_FILES} {CLIBS} {LOADLIBES} {EXTRALIBS}\
	{MAKEPEF} gdb{PROG_EXT} -o gdb {MAKEPEF_TOOL_FLAGS} {MAKEPEF_FLAGS}\
	{REZ} "{s}"mac-gdb.r -o gdb -append -d PROG_NAME='"'gdb'"' -d VERSION_STRING='"'{version}'"'\

# Replace the install actions with MPW-friendly script.
/^install \\Option-f /,/^$/c\
install \\Option-f all install-only\
\
install-only \\Option-f \
	NewFolderRecursive "{bindir}"\
	Duplicate -y gdb "{bindir}"gdb\
	If "`Exists SiowGDB`" != ""\
		Duplicate -y SiowGDB "{bindir}"SiowGDB\
	End If\


# Don't do any recursive subdir stuff.
/ subdir_do/s/{MAKE}/null-command/

# Edit out actions that only confuse MPW Make.
/^config.status \\Option-f/,/^$/d
/^Makefile \\Option-f/,/^$/d

# Don't test config.h dependencies.
/^"{o}"config.h \\Option-f/s/^/#/

# Add an action to build SIOWgdb.
$a\
SIOWgdb \\Option-f {OBS} {TSOBS} {ADD_DEPS} {CDEPS} "{o}"init.c.o\
	Delete -i -y SIOWgdb\
	{CC_LD} {INTERNAL_LDFLAGS} -t 'APPL' -c 'gdb ' -o SIOWgdb{PROG_EXT} "{o}"init.c.o {OBS} {TSOBS} {ADD_FILES} {CLIBS} {SIOW_LIB} {LOADLIBES} {EXTRALIBS}\
	{MAKEPEF} SIOWgdb{PROG_EXT} -o SIOWgdb -ft 'APPL' -fc 'gdb ' {MAKEPEF_FLAGS} \
	{REZ} -o SIOWgdb "{RIncludes}"siow.r -append -d __kPrefSize=5000 -d __kMinSize=2000 -d APPNAME='"'SIOWgdb'"' \
	{REZ} "{s}"mac-gdb.r -o SIOWgdb -append -d VERSION_STRING='"'{version}'"'\


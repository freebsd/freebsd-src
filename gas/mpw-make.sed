# Sed commands that finish translating the GAS Unix Makefile to MPW syntax.

/^# @target_frag@/a\
\
HDEFINES = \
LOCAL_LOADLIBES = \

/^srcroot = /s/^/#/
/^target_alias = /s/^/#/

/INCLUDES/s/-i "{srcdir}":\([a-z]*\)/-i "{topsrcdir}"\1/
/INCLUDES/s/-i "{srcdir}"\.\./-i "{topsrcdir}"/

/^INCLUDES = .*$/s/$/ -i "{topsrcdir}"include:mpw: -i ::extra-include:/

/$(TARG_CPU_DEP_@target_cpu_type@)/s/$(TARG_CPU_DEP_@target_cpu_type@)/{TARG_CPU_DEP}/

/@OPCODES_LIB@/s/@OPCODES_LIB@/::opcodes:libopcodes.o/
/@BFDLIB@/s/@BFDLIB@/::bfd:libbfd.o/

# Point at the libraries directly.
/@OPCODES_DEP@/s/@OPCODES_DEP@/::opcodes:libopcodes.o/
/@BFDDEP@/s/@BFDDEP@/::bfd:libbfd.o/

# Don't need this.
/@HLDFLAGS@/s/@HLDFLAGS@//

/extra_objects@/s/extra_objects@/{EXTRA_OBJECTS}/

/LOADLIBES/s/{LOADLIBES}/{EXTRALIBS}/

/@ALL_OBJ_DEPS@/s/@ALL_OBJ_DEPS@/::bfd:bfd.h/

# This causes problems - not sure why.
/^tags TAGS/,/etags /d

/^make-gas.com/s/^/#/

/true/s/ ; @true$//

# Remove references to conf.in, we don't need them.
/conf\.in/s/conf\.in//g

# Use _gdbinit everywhere instead of .gdbinit.
/gdbinit/s/\.gdbinit/_gdbinit/g

/atof-targ/s/"{s}"atof-targ\.c/"{o}"atof-targ.c/g
/config/s/"{s}"config\.h/"{o}"config.h/g
/config/s/^config\.h/"{o}"config.h/
/obj-format/s/"{s}"obj-format\.c/"{o}"obj-format.c/g
/obj-format/s/"{s}"obj-format\.h/"{o}"obj-format.h/g
/targ-cpu/s/"{s}"targ-cpu\.c/"{o}"targ-cpu.c/g
/targ-cpu/s/"{s}"targ-cpu\.h/"{o}"targ-cpu.h/g
/targ-env/s/"{s}"targ-env\.h/"{o}"targ-env.h/g

/m68k-parse.c/s/"{s}"m68k-parse\.c/"{o}"m68k-parse.c/g
/m68k-parse.c/s/^m68k-parse\.c/"{o}"m68k-parse.c/

# Whack out the config.h dependency, it only causes excess rebuilds.
/{OBJS}/s/{OBJS} \\Option-f "{o}"config.h/{OBJS} \\Option-f/

# ALL_CFLAGS includes TDEFINES, which is not desirable at link time.
/CC_LD/s/ALL_CFLAGS/CFLAGS/g

# The resource file is called mac-as.r.
/as.new.r/s/as\.new\.r/mac-as.r/

# ...and the PROG_NAME doesn't have a .new in it.
/PROG_NAME/s/PROG_NAME='"'as.new'"'/PROG_NAME='"'as'"'/

# Whack out recursive makes, they won't work.
/^[ 	][ 	]*srcroot=/,/^[ 	][ 	]*(cd /d

# Work around quoting problems by using multiple echo commands.
/'#define GAS_VERSION "{VERSION}"'/c\
	Echo -n '#define GAS_VERSION "' >> "{o}"config.new\
	Echo -n "{VERSION}" >> "{o}"config.new\
	Echo -n '"' >> "{o}"config.new

# Add a "stamps" target.
$a\
stamps \\Option-f config-stamp\

/^install \\Option-f/,/^$/c\
install \\Option-f all install-only\
\
install-only \\Option-f\
	NewFolderRecursive "{bindir}"\
	Duplicate -y :as.new "{bindir}"as\


# Whack out config-rebuilding targets, they won't work.
/^Makefile \\Option-f/,/^$/d
/^config.status \\Option-f/,/^$/d

/^"{o}"config.h \\Option-f/s/^/#/

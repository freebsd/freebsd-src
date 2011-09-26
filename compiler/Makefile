#
# iASL compiler/disassembler
#
# NOTE: This makefile is intended to be used in the Linux environment,
# with the Linux directory structure. It will not work directly
# on the native ACPICA source tree.
#

#
# Configuration
# Notes:
#   gcc should be version 4 or greater, otherwise some of the options
#       used will not be recognized.
#   Global optimization flags (such as -O2, -Os) are not used, since
#       they cause issues on some compilers.
#   The _GNU_SOURCE symbol is required for many hosts.
#
PROG = iasl

HOST =          _LINUX
NOMAN =         YES
COMPILE =       $(CC) -c $(CFLAGS) $(CWARNINGFLAGS) -o$@ $<

ACPICA_COMPONENTS =
ACPICA_SRC =            ..
ACPICA_INCLUDE =        $(ACPICA_SRC)/include
ACPICA_COMMON =         $(ACPICA_SRC)/common
ACPICA_CORE =           $(ACPICA_SRC)$(ACPICA_COMPONENTS)
ACPICA_TOOLS =          $(ACPICA_SRC)/tools
ACPICA_OSL =            $(ACPICA_SRC)/os_specific/service_layers
ACPICA_DEBUGGER =       $(ACPICA_CORE)/debugger
ACPICA_DISASSEMBLER =   $(ACPICA_CORE)/disassembler
ACPICA_DISPATCHER =     $(ACPICA_CORE)/dispatcher
ACPICA_EXECUTER =       $(ACPICA_CORE)/executer
ACPICA_NAMESPACE =      $(ACPICA_CORE)/namespace
ACPICA_PARSER =         $(ACPICA_CORE)/parser
ACPICA_TABLES =         $(ACPICA_CORE)/tables
ACPICA_UTILITIES =      $(ACPICA_CORE)/utilities
ASL_COMPILER =          $(ACPICA_SRC)/compiler
INSTALLDIR =            /usr/bin
INSTALLPROG =           cp --remove-destination $(PROG) $(INSTALLDIR)

ACPICA_HEADERS = \
    $(wildcard $(ACPICA_INCLUDE)/*.h) \
    $(wildcard $(ACPICA_INCLUDE)/platform/*.h)

#
# Search paths for source files
#
vpath %.c \
    $(ASL_COMPILER) \
    $(ACPICA_DEBUGGER) \
    $(ACPICA_DISASSEMBLER) \
    $(ACPICA_DISPATCHER) \
    $(ACPICA_EXECUTER) \
    $(ACPICA_NAMESPACE) \
    $(ACPICA_PARSER) \
    $(ACPICA_TABLES) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ASL_COMPILER)/*.h) \
    aslcompiler.y.h \
    dtparser.y.h

OBJECTS = \
	aslcompilerlex.o \
	aslcompilerparse.o \
	dtparserlex.o \
	dtparserparse.o \
	adfile.o \
	adisasm.o \
	adwalk.o \
	aslanalyze.o \
	aslbtypes.o \
	aslcodegen.o \
	aslcompile.o \
	aslerror.o \
	aslfiles.o \
	aslfold.o \
	asllength.o \
	asllisting.o \
	aslload.o \
	asllookup.o \
	aslmain.o \
	aslmap.o \
	aslopcodes.o \
	asloperands.o \
	aslopt.o \
	aslpredef.o \
	aslresource.o \
	aslrestype1.o \
	aslrestype1i.o \
	aslrestype2.o \
	aslrestype2d.o \
	aslrestype2e.o \
	aslrestype2q.o \
	aslrestype2w.o \
	aslstartup.o \
	aslstubs.o \
	asltransform.o \
	asltree.o \
	aslutils.o \
	asluuid.o \
	aslwalks.o \
	dtcompile.o \
	dtexpress.o \
	dtfield.o \
	dtio.o \
	dtsubtable.o \
	dttable.o \
	dttemplate.o \
	dtutils.o \
	dbfileio.o \
	dmbuffer.o \
	dmextern.o \
	dmnames.o \
	dmobject.o \
	dmopcode.o \
	dmresrc.o \
	dmresrcl.o \
	dmresrcs.o \
	dmrestag.o \
	dmtable.o \
	dmtbdump.o \
	dmtbinfo.o \
	dmutils.o \
	dmwalk.o \
	dsargs.o \
	dscontrol.o \
	dsfield.o \
	dsobject.o \
	dsopcode.o \
	dsutils.o \
	dswexec.o \
	dswload.o \
	dswload2.o \
	dswscope.o \
	dswstate.o \
	exconvrt.o \
	excreate.o \
	exdump.o \
	exmisc.o \
	exmutex.o \
	exnames.o \
	exoparg1.o \
	exoparg2.o \
	exoparg3.o \
	exoparg6.o \
	exprep.o \
	exregion.o \
	exresnte.o \
	exresolv.o \
	exresop.o \
	exstore.o \
	exstoren.o \
	exstorob.o \
	exsystem.o \
	exutils.o \
	getopt.o \
	nsaccess.o \
	nsalloc.o \
	nsdump.o \
	nsnames.o \
	nsobject.o \
	nsparse.o \
	nssearch.o \
	nsutils.o \
	nswalk.o \
	nsxfobj.o \
	osunixxf.o \
	psargs.o \
	psloop.o \
	psopcode.o \
	psparse.o \
	psscope.o \
	pstree.o \
	psutils.o \
	pswalk.o \
	tbfadt.o \
	tbinstal.o \
	tbutils.o \
	tbxface.o \
	utalloc.o \
	utcache.o \
	utcopy.o \
	utdebug.o \
	utdecode.o \
	utdelete.o \
	utglobal.o \
	utinit.o \
	utlock.o \
	utmath.o \
	utmisc.o \
	utmutex.o \
	utobject.o \
	utresrc.o \
	utstate.o \
	utxferror.o \
	utxface.o

INTERMEDIATES = \
	aslcompilerlex.c \
	aslcompilerparse.c \
	dtparserlex.c \
	dtparserparse.c

MISC = \
    aslcompilerparse.h \
	aslcompiler.y.h \
	aslcompilerparse.output \
    dtparserparse.h \
	dtparser.y.h \
	dtparserparse.output

CFLAGS+= \
    -D$(HOST) \
    -D_GNU_SOURCE \
    -DACPI_ASL_COMPILER \
    -I$(ACPICA_INCLUDE) \
    -I$(ASL_COMPILER)

CWARNINGFLAGS = \
    -ansi \
    -Wall \
    -Wbad-function-cast \
    -Wdeclaration-after-statement \
    -Werror \
    -Wformat=2 \
    -Wmissing-declarations \
    -Wmissing-prototypes \
    -Wstrict-aliasing=0 \
    -Wstrict-prototypes \
    -Wswitch-default \
    -Wpointer-arith \
    -Wundef

#
# gcc 4+ flags
#
CWARNINGFLAGS += \
    -Waddress \
    -Waggregate-return \
    -Wchar-subscripts \
    -Wempty-body \
    -Wlogical-op \
    -Wmissing-declarations \
    -Wmissing-field-initializers \
    -Wmissing-parameter-type \
    -Wnested-externs \
    -Wold-style-declaration \
    -Wold-style-definition \
    -Wredundant-decls \
    -Wtype-limits

#
# Bison/Flex configuration
#
YACC=    bison
YFLAGS+= -v -d -y

LEX=     flex
LFLAGS+= -i -s

#
# Root rule
#
$(PROG) : $(INTERMEDIATES) $(MISC) $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(PROG)

#
# Parser and Lexer - intermediate C files
#
aslcompilerlex.c :   $(ASL_COMPILER)/aslcompiler.l
	${LEX} ${LFLAGS} -PAslCompiler -o$@ $?

aslcompilerparse.c aslcompilerparse.h : $(ASL_COMPILER)/aslcompiler.y
	${YACC} ${YFLAGS} -pAslCompiler -o$@ $?

dtparserlex.c :      $(ASL_COMPILER)/dtparser.l
	${LEX} ${LFLAGS} -PDtParser -o$@ $?

dtparserparse.c dtparserparse.h : $(ASL_COMPILER)/dtparser.y
	${YACC} ${YFLAGS} -pDtParser -o$@ $?

# Rename headers produced by bison/yacc

dtparser.y.h: dtparserparse.h
	@echo Copy intermediate file:
	@cp -f -v dtparserparse.h dtparser.y.h

aslcompiler.y.h : aslcompilerparse.h
	@echo Copy intermediate file:
	@cp -f -v aslcompilerparse.h aslcompiler.y.h


#
# Parsers and Lexers - final object files
#
# Cannot use the common compile warning flags since the C files are created
# by the utilities above and they are not necessarily ANSI C, etc.
#
aslcompilerlex.o :   aslcompilerlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -Wstrict-aliasing=0 -o$@ $?

aslcompilerparse.o : aslcompilerparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -Wstrict-aliasing=0 -o$@ $?

dtparserlex.o :      dtparserlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -Wstrict-aliasing=0 -o$@ $?

dtparserparse.o :    dtparserparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -Wstrict-aliasing=0 -o$@ $?


%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS) $(INTERMEDIATES) $(MISC)

install :
	$(INSTALLPROG)

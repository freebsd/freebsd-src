#
# iASL compiler/disassembler
#

#
# Note: This makefile is intended to be used from within the native
# ACPICA directory structure, from under generate/unix. It specifically
# places all object files in a generate/unix subdirectory, not within
# the various ACPICA source directories. This prevents collisions
# between different compilations of the same source file with different
# compile options, and prevents pollution of the source code.
#
include ../Makefile.config
PROG = iasl

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

#
# Flags specific to iASL compiler
#
CFLAGS+= \
    -DACPI_ASL_COMPILER \
    -I$(ASL_COMPILER) \
    -I.

#
# Root rule
#
$(PROG) : $(INTERMEDIATES) $(MISC) $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROG)
	$(COPYPROG)

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
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

aslcompilerparse.o : aslcompilerparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

dtparserlex.o :      dtparserlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

dtparserparse.o :    dtparserparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?


%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS) $(INTERMEDIATES) $(MISC)

install :
	$(INSTALLPROG)

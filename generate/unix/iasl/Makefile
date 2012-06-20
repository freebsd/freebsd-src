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
PROG = $(OBJDIR)/iasl

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
    $(OBJDIR)/aslcompiler.y.h \
    $(OBJDIR)/dtparser.y.h \
    $(OBJDIR)/prparser.y.h

OBJECTS = \
	$(OBJDIR)/aslcompilerlex.o \
	$(OBJDIR)/aslcompilerparse.o \
	$(OBJDIR)/dtparserlex.o \
	$(OBJDIR)/dtparserparse.o \
	$(OBJDIR)/prparserlex.o \
	$(OBJDIR)/prparserparse.o \
	$(OBJDIR)/adfile.o \
	$(OBJDIR)/adisasm.o \
	$(OBJDIR)/adwalk.o \
	$(OBJDIR)/ahpredef.o \
	$(OBJDIR)/aslanalyze.o \
	$(OBJDIR)/aslbtypes.o \
	$(OBJDIR)/aslcodegen.o \
	$(OBJDIR)/aslcompile.o \
	$(OBJDIR)/aslerror.o \
	$(OBJDIR)/aslfiles.o \
	$(OBJDIR)/aslfold.o \
	$(OBJDIR)/asllength.o \
	$(OBJDIR)/asllisting.o \
	$(OBJDIR)/aslload.o \
	$(OBJDIR)/asllookup.o \
	$(OBJDIR)/aslmain.o \
	$(OBJDIR)/aslmap.o \
	$(OBJDIR)/aslopcodes.o \
	$(OBJDIR)/asloperands.o \
	$(OBJDIR)/aslopt.o \
	$(OBJDIR)/aslpredef.o \
	$(OBJDIR)/aslresource.o \
	$(OBJDIR)/aslrestype1.o \
	$(OBJDIR)/aslrestype1i.o \
	$(OBJDIR)/aslrestype2.o \
	$(OBJDIR)/aslrestype2d.o \
	$(OBJDIR)/aslrestype2e.o \
	$(OBJDIR)/aslrestype2q.o \
	$(OBJDIR)/aslrestype2s.o \
	$(OBJDIR)/aslrestype2w.o \
	$(OBJDIR)/aslstartup.o \
	$(OBJDIR)/aslstubs.o \
	$(OBJDIR)/asltransform.o \
	$(OBJDIR)/asltree.o \
	$(OBJDIR)/aslutils.o \
	$(OBJDIR)/asluuid.o \
	$(OBJDIR)/aslwalks.o \
	$(OBJDIR)/dtcompile.o \
	$(OBJDIR)/dtexpress.o \
	$(OBJDIR)/dtfield.o \
	$(OBJDIR)/dtio.o \
	$(OBJDIR)/dtsubtable.o \
	$(OBJDIR)/dttable.o \
	$(OBJDIR)/dttemplate.o \
	$(OBJDIR)/dtutils.o \
	$(OBJDIR)/dbfileio.o \
	$(OBJDIR)/dmbuffer.o \
	$(OBJDIR)/dmextern.o \
	$(OBJDIR)/dmnames.o \
	$(OBJDIR)/dmobject.o \
	$(OBJDIR)/dmopcode.o \
	$(OBJDIR)/dmresrc.o \
	$(OBJDIR)/dmresrcl.o \
	$(OBJDIR)/dmresrcl2.o \
	$(OBJDIR)/dmresrcs.o \
	$(OBJDIR)/dmrestag.o \
	$(OBJDIR)/dmtable.o \
	$(OBJDIR)/dmtbdump.o \
	$(OBJDIR)/dmtbinfo.o \
	$(OBJDIR)/dmutils.o \
	$(OBJDIR)/dmwalk.o \
	$(OBJDIR)/dsargs.o \
	$(OBJDIR)/dscontrol.o \
	$(OBJDIR)/dsfield.o \
	$(OBJDIR)/dsobject.o \
	$(OBJDIR)/dsopcode.o \
	$(OBJDIR)/dsutils.o \
	$(OBJDIR)/dswexec.o \
	$(OBJDIR)/dswload.o \
	$(OBJDIR)/dswload2.o \
	$(OBJDIR)/dswscope.o \
	$(OBJDIR)/dswstate.o \
	$(OBJDIR)/exconvrt.o \
	$(OBJDIR)/excreate.o \
	$(OBJDIR)/exdump.o \
	$(OBJDIR)/exmisc.o \
	$(OBJDIR)/exmutex.o \
	$(OBJDIR)/exnames.o \
	$(OBJDIR)/exoparg1.o \
	$(OBJDIR)/exoparg2.o \
	$(OBJDIR)/exoparg3.o \
	$(OBJDIR)/exoparg6.o \
	$(OBJDIR)/exprep.o \
	$(OBJDIR)/exregion.o \
	$(OBJDIR)/exresnte.o \
	$(OBJDIR)/exresolv.o \
	$(OBJDIR)/exresop.o \
	$(OBJDIR)/exstore.o \
	$(OBJDIR)/exstoren.o \
	$(OBJDIR)/exstorob.o \
	$(OBJDIR)/exsystem.o \
	$(OBJDIR)/exutils.o \
	$(OBJDIR)/getopt.o \
	$(OBJDIR)/nsaccess.o \
	$(OBJDIR)/nsalloc.o \
	$(OBJDIR)/nsdump.o \
	$(OBJDIR)/nsnames.o \
	$(OBJDIR)/nsobject.o \
	$(OBJDIR)/nsparse.o \
	$(OBJDIR)/nssearch.o \
	$(OBJDIR)/nsutils.o \
	$(OBJDIR)/nswalk.o \
	$(OBJDIR)/nsxfobj.o \
	$(OBJDIR)/osunixxf.o \
	$(OBJDIR)/prexpress.o \
	$(OBJDIR)/prmacros.o \
	$(OBJDIR)/prscan.o \
	$(OBJDIR)/prutils.o \
	$(OBJDIR)/psargs.o \
	$(OBJDIR)/psloop.o \
	$(OBJDIR)/psopcode.o \
	$(OBJDIR)/psparse.o \
	$(OBJDIR)/psscope.o \
	$(OBJDIR)/pstree.o \
	$(OBJDIR)/psutils.o \
	$(OBJDIR)/pswalk.o \
	$(OBJDIR)/tbfadt.o \
	$(OBJDIR)/tbinstal.o \
	$(OBJDIR)/tbutils.o \
	$(OBJDIR)/tbxface.o \
	$(OBJDIR)/utaddress.o \
	$(OBJDIR)/utalloc.o \
	$(OBJDIR)/utcache.o \
	$(OBJDIR)/utcopy.o \
	$(OBJDIR)/utdebug.o \
	$(OBJDIR)/utdecode.o \
	$(OBJDIR)/utdelete.o \
	$(OBJDIR)/utglobal.o \
	$(OBJDIR)/utinit.o \
	$(OBJDIR)/utlock.o \
	$(OBJDIR)/utmath.o \
	$(OBJDIR)/utmisc.o \
	$(OBJDIR)/utmutex.o \
	$(OBJDIR)/utobject.o \
	$(OBJDIR)/utresrc.o \
	$(OBJDIR)/utstate.o \
	$(OBJDIR)/utxferror.o \
	$(OBJDIR)/utxface.o

INTERMEDIATES = \
	$(OBJDIR)/aslcompilerlex.c \
	$(OBJDIR)/aslcompilerparse.c \
	$(OBJDIR)/dtparserlex.c \
	$(OBJDIR)/dtparserparse.c \
	$(OBJDIR)/prparserlex.c \
	$(OBJDIR)/prparserparse.c

MISC = \
   	$(OBJDIR)/aslcompilerparse.h \
	$(OBJDIR)/aslcompiler.y.h \
	$(OBJDIR)/aslcompilerparse.output \
   	$(OBJDIR)/dtparserparse.h \
	$(OBJDIR)/dtparser.y.h \
	$(OBJDIR)/dtparserparse.output \
   	$(OBJDIR)/prparserparse.h \
	$(OBJDIR)/prparser.y.h \
	$(OBJDIR)/prparserparse.output

#
# Flags specific to iASL compiler
#
CFLAGS+= \
    -DACPI_ASL_COMPILER \
    -I$(ASL_COMPILER) \
    -I$(OBJDIR)

#
# Root rule
#
$(PROG) : $(INTERMEDIATES) $(MISC) $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROG)
	$(COPYPROG)

#
# Parser and Lexer - intermediate C files
#
$(OBJDIR)/aslcompilerlex.c :   $(ASL_COMPILER)/aslcompiler.l $(ASL_COMPILER)/aslsupport.l
	${LEX} ${LFLAGS} -PAslCompiler -o$@ $(ASL_COMPILER)/aslcompiler.l

$(OBJDIR)/aslcompilerparse.c $(OBJDIR)/aslcompilerparse.h : $(ASL_COMPILER)/aslcompiler.y
	${YACC} ${YFLAGS} -pAslCompiler -o$@ $?

$(OBJDIR)/dtparserlex.c :      $(ASL_COMPILER)/dtparser.l
	${LEX} ${LFLAGS} -PDtParser -o$@ $?

$(OBJDIR)/dtparserparse.c $(OBJDIR)/dtparserparse.h : $(ASL_COMPILER)/dtparser.y
	${YACC} ${YFLAGS} -pDtParser -o$@ $?

$(OBJDIR)/prparserlex.c :      $(ASL_COMPILER)/prparser.l
	${LEX} ${LFLAGS} -PPrParser -o$@ $?

$(OBJDIR)/prparserparse.c $(OBJDIR)/prparserparse.h : $(ASL_COMPILER)/prparser.y
	${YACC} ${YFLAGS} -pPrParser -o$@ $?


# Rename headers produced by bison/yacc

$(OBJDIR)/aslcompiler.y.h : $(OBJDIR)/aslcompilerparse.h
	@echo Copy intermediate file:
	@cp -f -v $(OBJDIR)/aslcompilerparse.h $(OBJDIR)/aslcompiler.y.h

$(OBJDIR)/dtparser.y.h: $(OBJDIR)/dtparserparse.h
	@echo Copy intermediate file:
	@cp -f -v $(OBJDIR)/dtparserparse.h $(OBJDIR)/dtparser.y.h

$(OBJDIR)/prparser.y.h: $(OBJDIR)/prparserparse.h
	@echo Copy intermediate file:
	@cp -f -v $(OBJDIR)/prparserparse.h $(OBJDIR)/prparser.y.h


#
# Parsers and Lexers - final object files
#
# Cannot use the common compile warning flags since the C files are created
# by the utilities above and they are not necessarily ANSI C, etc.
#
$(OBJDIR)/aslcompilerlex.o :   $(OBJDIR)/aslcompilerlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

$(OBJDIR)/aslcompilerparse.o : $(OBJDIR)/aslcompilerparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

$(OBJDIR)/dtparserlex.o :      $(OBJDIR)/dtparserlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

$(OBJDIR)/dtparserparse.o :    $(OBJDIR)/dtparserparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

$(OBJDIR)/prparserlex.o :      $(OBJDIR)/prparserlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?

$(OBJDIR)/prparserparse.o :    $(OBJDIR)/prparserparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $?


$(OBJDIR)/%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS) $(INTERMEDIATES) $(MISC)

install :
	$(INSTALLPROG)

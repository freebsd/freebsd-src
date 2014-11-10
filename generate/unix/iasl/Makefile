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
FINAL_PROG = ../$(BINDIR)/iasl
PROG = $(OBJDIR)/iasl

#
# Search paths for source files
#
vpath %.c \
    $(ASL_COMPILER)\
    $(ACPICA_DEBUGGER)\
    $(ACPICA_DISASSEMBLER)\
    $(ACPICA_DISPATCHER)\
    $(ACPICA_EXECUTER)\
    $(ACPICA_NAMESPACE)\
    $(ACPICA_PARSER)\
    $(ACPICA_TABLES)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ASL_COMPILER)/*.h)\
    $(OBJDIR)/aslcompiler.y.h\
    $(OBJDIR)/dtparser.y.h\
    $(OBJDIR)/prparser.y.h

OBJECTS = \
	$(OBJDIR)/aslcompilerlex.o\
	$(OBJDIR)/aslcompilerparse.o\
	$(OBJDIR)/dtparserlex.o\
	$(OBJDIR)/dtparserparse.o\
	$(OBJDIR)/prparserlex.o\
	$(OBJDIR)/prparserparse.o\
	$(OBJDIR)/adfile.o\
	$(OBJDIR)/adisasm.o\
	$(OBJDIR)/adwalk.o\
	$(OBJDIR)/ahids.o\
	$(OBJDIR)/ahpredef.o\
	$(OBJDIR)/ahuuids.o\
	$(OBJDIR)/aslanalyze.o\
	$(OBJDIR)/aslascii.o\
	$(OBJDIR)/aslbtypes.o\
	$(OBJDIR)/aslcodegen.o\
	$(OBJDIR)/aslcompile.o\
	$(OBJDIR)/aslerror.o\
	$(OBJDIR)/aslfiles.o\
	$(OBJDIR)/aslfileio.o\
	$(OBJDIR)/aslfold.o\
	$(OBJDIR)/aslhex.o\
	$(OBJDIR)/asllength.o\
	$(OBJDIR)/asllisting.o\
	$(OBJDIR)/asllistsup.o\
	$(OBJDIR)/aslload.o\
	$(OBJDIR)/asllookup.o\
	$(OBJDIR)/aslmain.o\
	$(OBJDIR)/aslmap.o\
	$(OBJDIR)/aslmapenter.o\
	$(OBJDIR)/aslmapoutput.o\
	$(OBJDIR)/aslmaputils.o\
	$(OBJDIR)/aslmessages.o\
	$(OBJDIR)/aslmethod.o\
	$(OBJDIR)/aslnamesp.o\
	$(OBJDIR)/asloffset.o\
	$(OBJDIR)/aslopcodes.o\
	$(OBJDIR)/asloperands.o\
	$(OBJDIR)/aslopt.o\
	$(OBJDIR)/asloptions.o\
	$(OBJDIR)/aslpredef.o\
	$(OBJDIR)/aslprepkg.o\
	$(OBJDIR)/aslprintf.o\
	$(OBJDIR)/aslprune.o\
	$(OBJDIR)/aslresource.o\
	$(OBJDIR)/aslrestype1.o\
	$(OBJDIR)/aslrestype1i.o\
	$(OBJDIR)/aslrestype2.o\
	$(OBJDIR)/aslrestype2d.o\
	$(OBJDIR)/aslrestype2e.o\
	$(OBJDIR)/aslrestype2q.o\
	$(OBJDIR)/aslrestype2s.o\
	$(OBJDIR)/aslrestype2w.o\
	$(OBJDIR)/aslstartup.o\
	$(OBJDIR)/aslstubs.o\
	$(OBJDIR)/asltransform.o\
	$(OBJDIR)/asltree.o\
	$(OBJDIR)/aslutils.o\
	$(OBJDIR)/asluuid.o\
	$(OBJDIR)/aslwalks.o\
	$(OBJDIR)/aslxref.o\
	$(OBJDIR)/cmfsize.o\
	$(OBJDIR)/dbfileio.o\
	$(OBJDIR)/dmbuffer.o\
	$(OBJDIR)/dmcstyle.o\
	$(OBJDIR)/dmdeferred.o\
	$(OBJDIR)/dmextern.o\
	$(OBJDIR)/dmnames.o\
	$(OBJDIR)/dmobject.o\
	$(OBJDIR)/dmopcode.o\
	$(OBJDIR)/dmresrc.o\
	$(OBJDIR)/dmresrcl.o\
	$(OBJDIR)/dmresrcl2.o\
	$(OBJDIR)/dmresrcs.o\
	$(OBJDIR)/dmrestag.o\
	$(OBJDIR)/dmtable.o\
	$(OBJDIR)/dmtbdump.o\
	$(OBJDIR)/dmtbinfo.o\
	$(OBJDIR)/dmutils.o\
	$(OBJDIR)/dmwalk.o\
	$(OBJDIR)/dsargs.o\
	$(OBJDIR)/dscontrol.o\
	$(OBJDIR)/dsfield.o\
	$(OBJDIR)/dsobject.o\
	$(OBJDIR)/dsopcode.o\
	$(OBJDIR)/dsutils.o\
	$(OBJDIR)/dswexec.o\
	$(OBJDIR)/dswload.o\
	$(OBJDIR)/dswload2.o\
	$(OBJDIR)/dswscope.o\
	$(OBJDIR)/dswstate.o\
	$(OBJDIR)/dtcompile.o\
	$(OBJDIR)/dtexpress.o\
	$(OBJDIR)/dtfield.o\
	$(OBJDIR)/dtio.o\
	$(OBJDIR)/dtsubtable.o\
	$(OBJDIR)/dttable.o\
	$(OBJDIR)/dttemplate.o\
	$(OBJDIR)/dtutils.o\
	$(OBJDIR)/exconvrt.o\
	$(OBJDIR)/excreate.o\
	$(OBJDIR)/exdump.o\
	$(OBJDIR)/exmisc.o\
	$(OBJDIR)/exmutex.o\
	$(OBJDIR)/exnames.o\
	$(OBJDIR)/exoparg1.o\
	$(OBJDIR)/exoparg2.o\
	$(OBJDIR)/exoparg3.o\
	$(OBJDIR)/exoparg6.o\
	$(OBJDIR)/exprep.o\
	$(OBJDIR)/exregion.o\
	$(OBJDIR)/exresnte.o\
	$(OBJDIR)/exresolv.o\
	$(OBJDIR)/exresop.o\
	$(OBJDIR)/exstore.o\
	$(OBJDIR)/exstoren.o\
	$(OBJDIR)/exstorob.o\
	$(OBJDIR)/exsystem.o\
	$(OBJDIR)/exutils.o\
	$(OBJDIR)/getopt.o\
	$(OBJDIR)/nsaccess.o\
	$(OBJDIR)/nsalloc.o\
	$(OBJDIR)/nsdump.o\
	$(OBJDIR)/nsnames.o\
	$(OBJDIR)/nsobject.o\
	$(OBJDIR)/nsparse.o\
	$(OBJDIR)/nssearch.o\
	$(OBJDIR)/nsutils.o\
	$(OBJDIR)/nswalk.o\
	$(OBJDIR)/nsxfobj.o\
	$(OBJDIR)/osunixxf.o\
	$(OBJDIR)/oslibcfs.o\
	$(OBJDIR)/prexpress.o\
	$(OBJDIR)/prmacros.o\
	$(OBJDIR)/prscan.o\
	$(OBJDIR)/prutils.o\
	$(OBJDIR)/psargs.o\
	$(OBJDIR)/psloop.o\
	$(OBJDIR)/psobject.o\
	$(OBJDIR)/psopcode.o\
	$(OBJDIR)/psopinfo.o\
	$(OBJDIR)/psparse.o\
	$(OBJDIR)/psscope.o\
	$(OBJDIR)/pstree.o\
	$(OBJDIR)/psutils.o\
	$(OBJDIR)/pswalk.o\
	$(OBJDIR)/tbdata.o\
	$(OBJDIR)/tbfadt.o\
	$(OBJDIR)/tbinstal.o\
	$(OBJDIR)/tbprint.o\
	$(OBJDIR)/tbutils.o\
	$(OBJDIR)/tbxface.o\
	$(OBJDIR)/utaddress.o\
	$(OBJDIR)/utalloc.o\
	$(OBJDIR)/utbuffer.o\
	$(OBJDIR)/utcache.o\
	$(OBJDIR)/utcopy.o\
	$(OBJDIR)/utdebug.o\
	$(OBJDIR)/utdecode.o\
	$(OBJDIR)/utdelete.o\
	$(OBJDIR)/uterror.o\
	$(OBJDIR)/utexcep.o\
	$(OBJDIR)/utfileio.o\
	$(OBJDIR)/utglobal.o\
	$(OBJDIR)/uthex.o\
	$(OBJDIR)/utinit.o\
	$(OBJDIR)/utlock.o\
	$(OBJDIR)/utmath.o\
	$(OBJDIR)/utmisc.o\
	$(OBJDIR)/utmutex.o\
	$(OBJDIR)/utobject.o\
	$(OBJDIR)/utownerid.o\
	$(OBJDIR)/utpredef.o\
	$(OBJDIR)/utprint.o\
	$(OBJDIR)/utresrc.o\
	$(OBJDIR)/utstate.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utuuid.o\
	$(OBJDIR)/utxface.o\
	$(OBJDIR)/utxferror.o

INTERMEDIATES = \
	$(OBJDIR)/aslcompiler.y\
	$(OBJDIR)/aslcompilerlex.c\
	$(OBJDIR)/aslcompilerparse.c\
	$(OBJDIR)/dtparserlex.c\
	$(OBJDIR)/dtparserparse.c\
	$(OBJDIR)/prparserlex.c\
	$(OBJDIR)/prparserparse.c

MISC = \
	$(OBJDIR)/aslcompiler.y.h\
	$(OBJDIR)/dtparser.y.h\
	$(OBJDIR)/prparser.y.h

ASL_PARSER = \
	$(ASL_COMPILER)/aslparser.y\
	$(ASL_COMPILER)/aslsupport.y\
	$(ASL_COMPILER)/asltokens.y\
	$(ASL_COMPILER)/asltypes.y\
	$(ASL_COMPILER)/aslrules.y

ASL_LEXER = \
	$(ASL_COMPILER)/aslcompiler.l\
	$(ASL_COMPILER)/aslsupport.l\
	$(OBJDIR)/aslcompiler.y.h


#
# Flags specific to iASL compiler
#
CFLAGS += \
    -DACPI_ASL_COMPILER\
    -I$(ASL_COMPILER)\
    -I$(OBJDIR)

#
# Common Rules
#
include ../Makefile.rules

#
# Macro processing for iASL .y files
#
$(OBJDIR)/aslcompiler.y :      $(ASL_PARSER)
	$(MACROPROC) $(MFLAGS) $(ASL_COMPILER)/aslparser.y > $(OBJDIR)/aslcompiler.y

#
# Parser and Lexer - intermediate C files
#
$(OBJDIR)/aslcompilerlex.c :   $(ASL_LEXER)
	$(LEX) $(LFLAGS) -PAslCompiler -o$@ $(ASL_COMPILER)/aslcompiler.l

$(OBJDIR)/aslcompiler.y.h :    $(OBJDIR)/aslcompiler.y
	$(YACC) $(YFLAGS) -pAslCompiler -o/dev/null --defines=$@ $<

$(OBJDIR)/aslcompilerparse.c : $(OBJDIR)/aslcompiler.y
	$(YACC) $(YFLAGS) -pAslCompiler -o$@ --defines=/dev/null $<

$(OBJDIR)/dtparserlex.c :      $(ASL_COMPILER)/dtparser.l $(OBJDIR)/dtparser.y.h
	$(LEX) $(LFLAGS) -PDtParser -o$@ $<

$(OBJDIR)/dtparser.y.h :       $(ASL_COMPILER)/dtparser.y
	$(YACC) $(YFLAGS) -pDtParser -o/dev/null --defines=$@ $<

$(OBJDIR)/dtparserparse.c :    $(ASL_COMPILER)/dtparser.y
	$(YACC) $(YFLAGS) -pDtParser -o$@ --defines=/dev/null $<

$(OBJDIR)/prparserlex.c :      $(ASL_COMPILER)/prparser.l $(OBJDIR)/prparser.y.h
	$(LEX) $(LFLAGS) -PPrParser -o$@ $<

$(OBJDIR)/prparser.y.h :       $(ASL_COMPILER)/prparser.y
	$(YACC) $(YFLAGS) -pPrParser -o/dev/null --defines=$@ $<

$(OBJDIR)/prparserparse.c :    $(ASL_COMPILER)/prparser.y
	$(YACC) $(YFLAGS) -pPrParser -o$@ --defines=/dev/null $<


#
# Parsers and Lexers - final object files
#
# Cannot use the common compile warning flags since the C files are created
# by the utilities above and they are not necessarily ANSI C, etc.
#
$(OBJDIR)/aslcompilerlex.o :   $(OBJDIR)/aslcompilerlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $<

$(OBJDIR)/aslcompilerparse.o : $(OBJDIR)/aslcompilerparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $<

$(OBJDIR)/dtparserlex.o :      $(OBJDIR)/dtparserlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $<

$(OBJDIR)/dtparserparse.o :    $(OBJDIR)/dtparserparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $<

$(OBJDIR)/prparserlex.o :      $(OBJDIR)/prparserlex.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $<

$(OBJDIR)/prparserparse.o :    $(OBJDIR)/prparserparse.c
	$(CC) -c $(CFLAGS) -Wall -Werror -o$@ $<

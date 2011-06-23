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
COMPILE =       $(CC) -c $(CFLAGS) $(CWARNINGFLAGS) -o$@ $?

ACPICA_COMPONENTS =
ACPICA_SRC =    ..
ACPICA_COMMON = $(ACPICA_SRC)/common
ACPICA_CORE =   $(ACPICA_SRC)$(ACPICA_COMPONENTS)
ACPICA_TOOLS =  $(ACPICA_SRC)/tools
ACPICA_OSL =    $(ACPICA_SRC)/os_specific/service_layers
ASL_COMPILER =  $(ACPICA_SRC)/compiler
INSTALLDIR =    /usr/bin
INSTALLPROG =   cp --remove-destination $(PROG) $(INSTALLDIR)

CFLAGS+= \
    -D$(HOST) \
    -D_GNU_SOURCE \
    -DACPI_ASL_COMPILER \
    -I$(ACPICA_SRC)/include \
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
# Bison/Flex configuration
#
YACC=    bison
YFLAGS+= -v -d -y

LEX=     flex
LFLAGS+= -i -s

OBJS = \
	adfile.o \
	adisasm.o \
	adwalk.o \
	aslanalyze.o \
	aslbtypes.o \
	aslcodegen.o \
	aslcompile.o \
	aslcompilerlex.o \
	aslcompilerparse.o \
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
	dtparserlex.o \
	dtparserparse.o \
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
	aslcompiler.y.h \
	aslcompilerparse.output \
	dtparser.y.h \
	dtparserparse.output


#
# Root rule
#
$(PROG) : $(INTERMEDIATES) $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(PROG)


#
# Parser and Lexer - intermediate C files
#
aslcompilerlex.c :   $(ASL_COMPILER)/aslcompiler.l
	${LEX} ${LFLAGS} -PAslCompiler -o$@ $?

aslcompilerparse.c : $(ASL_COMPILER)/aslcompiler.y
	${YACC} ${YFLAGS} -pAslCompiler -o$@ $?
	@mv -f aslcompilerparse.h aslcompiler.y.h

dtparserlex.c :      $(ASL_COMPILER)/dtparser.l
	${LEX} ${LFLAGS} -PDtParser -o$@ $?

dtparserparse.c :    $(ASL_COMPILER)/dtparser.y
	${YACC} ${YFLAGS} -pDtParser -o$@ $?
	@mv -f dtparserparse.h dtparser.y.h


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


#
# Compiler source
#
aslanalyze.o :      $(ASL_COMPILER)/aslanalyze.c
	$(COMPILE)

aslbtypes.o :       $(ASL_COMPILER)/aslbtypes.c
	$(COMPILE)

aslcodegen.o :      $(ASL_COMPILER)/aslcodegen.c
	$(COMPILE)

aslcompile.o :      $(ASL_COMPILER)/aslcompile.c
	$(COMPILE)

aslerror.o :        $(ASL_COMPILER)/aslerror.c
	$(COMPILE)

aslfiles.o :        $(ASL_COMPILER)/aslfiles.c
	$(COMPILE)

aslfold.o :         $(ASL_COMPILER)/aslfold.c
	$(COMPILE)

asllength.o :       $(ASL_COMPILER)/asllength.c
	$(COMPILE)

asllisting.o :      $(ASL_COMPILER)/asllisting.c
	$(COMPILE)

aslload.o :         $(ASL_COMPILER)/aslload.c
	$(COMPILE)

asllookup.o :       $(ASL_COMPILER)/asllookup.c
	$(COMPILE)

aslmain.o :         $(ASL_COMPILER)/aslmain.c
	$(COMPILE)

aslmap.o :          $(ASL_COMPILER)/aslmap.c
	$(COMPILE)

aslopcodes.o :      $(ASL_COMPILER)/aslopcodes.c
	$(COMPILE)

asloperands.o :     $(ASL_COMPILER)/asloperands.c
	$(COMPILE)

aslopt.o :          $(ASL_COMPILER)/aslopt.c
	$(COMPILE)

aslpredef.o :       $(ASL_COMPILER)/aslpredef.c
	$(COMPILE)

aslresource.o :     $(ASL_COMPILER)/aslresource.c
	$(COMPILE)

aslrestype1.o :     $(ASL_COMPILER)/aslrestype1.c
	$(COMPILE)

aslrestype1i.o :    $(ASL_COMPILER)/aslrestype1i.c
	$(COMPILE)

aslrestype2.o :     $(ASL_COMPILER)/aslrestype2.c
	$(COMPILE)

aslrestype2d.o :    $(ASL_COMPILER)/aslrestype2d.c
	$(COMPILE)

aslrestype2e.o :    $(ASL_COMPILER)/aslrestype2e.c
	$(COMPILE)

aslrestype2q.o :    $(ASL_COMPILER)/aslrestype2q.c
	$(COMPILE)

aslrestype2w.o :    $(ASL_COMPILER)/aslrestype2w.c
	$(COMPILE)

aslstartup.o :      $(ASL_COMPILER)/aslstartup.c
	$(COMPILE)

aslstubs.o :        $(ASL_COMPILER)/aslstubs.c
	$(COMPILE)

asltransform.o :    $(ASL_COMPILER)/asltransform.c
	$(COMPILE)

asltree.o :         $(ASL_COMPILER)/asltree.c
	$(COMPILE)

aslutils.o :        $(ASL_COMPILER)/aslutils.c
	$(COMPILE)

asluuid.o :         $(ASL_COMPILER)/asluuid.c
	$(COMPILE)

aslwalks.o :        $(ASL_COMPILER)/aslwalks.c
	$(COMPILE)


#
# Data Table Compiler
#
dtcompile.o :       $(ASL_COMPILER)/dtcompile.c
	$(COMPILE)

dtexpress.o :       $(ASL_COMPILER)/dtexpress.c
	$(COMPILE)

dtfield.o :         $(ASL_COMPILER)/dtfield.c
	$(COMPILE)

dtio.o :            $(ASL_COMPILER)/dtio.c
	$(COMPILE)

dtsubtable.o :      $(ASL_COMPILER)/dtsubtable.c
	$(COMPILE)

dttable.o :         $(ASL_COMPILER)/dttable.c
	$(COMPILE)

dttemplate.o :      $(ASL_COMPILER)/dttemplate.c
	$(COMPILE)

dtutils.o :         $(ASL_COMPILER)/dtutils.c
	$(COMPILE)


#
# ACPICA core source - common
#
adfile.o :          $(ACPICA_COMMON)/adfile.c
	$(COMPILE)

adisasm.o :         $(ACPICA_COMMON)/adisasm.c
	$(COMPILE)

adwalk.o :          $(ACPICA_COMMON)/adwalk.c
	$(COMPILE)

dmextern.o :        $(ACPICA_COMMON)/dmextern.c
	$(COMPILE)

dmrestag.o :        $(ACPICA_COMMON)/dmrestag.c
	$(COMPILE)

dmtable.o :         $(ACPICA_COMMON)/dmtable.c
	$(COMPILE)

dmtbdump.o :        $(ACPICA_COMMON)/dmtbdump.c
	$(COMPILE)

dmtbinfo.o :        $(ACPICA_COMMON)/dmtbinfo.c
	$(COMPILE)

getopt.o :          $(ACPICA_COMMON)/getopt.c
	$(COMPILE)


#
# ACPICA core source
#
dbfileio.o :        $(ACPICA_CORE)/debugger/dbfileio.c
	$(COMPILE)

dmbuffer.o :        $(ACPICA_CORE)/disassembler/dmbuffer.c
	$(COMPILE)

dmnames.o :         $(ACPICA_CORE)/disassembler/dmnames.c
	$(COMPILE)

dmobject.o :        $(ACPICA_CORE)/disassembler/dmobject.c
	$(COMPILE)

dmopcode.o :        $(ACPICA_CORE)/disassembler/dmopcode.c
	$(COMPILE)

dmresrc.o :         $(ACPICA_CORE)/disassembler/dmresrc.c
	$(COMPILE)

dmresrcl.o :        $(ACPICA_CORE)/disassembler/dmresrcl.c
	$(COMPILE)

dmresrcs.o :        $(ACPICA_CORE)/disassembler/dmresrcs.c
	$(COMPILE)

dmutils.o :         $(ACPICA_CORE)/disassembler/dmutils.c
	$(COMPILE)

dmwalk.o :          $(ACPICA_CORE)/disassembler/dmwalk.c
	$(COMPILE)

dsargs.o :          $(ACPICA_CORE)/dispatcher/dsargs.c
	$(COMPILE)

dscontrol.o :       $(ACPICA_CORE)/dispatcher/dscontrol.c
	$(COMPILE)

dsfield.o :         $(ACPICA_CORE)/dispatcher/dsfield.c
	$(COMPILE)

dsobject.o :        $(ACPICA_CORE)/dispatcher/dsobject.c
	$(COMPILE)

dsopcode.o :        $(ACPICA_CORE)/dispatcher/dsopcode.c
	$(COMPILE)

dsutils.o :         $(ACPICA_CORE)/dispatcher/dsutils.c
	$(COMPILE)

dswexec.o :         $(ACPICA_CORE)/dispatcher/dswexec.c
	$(COMPILE)

dswload.o :         $(ACPICA_CORE)/dispatcher/dswload.c
	$(COMPILE)

dswload2.o :        $(ACPICA_CORE)/dispatcher/dswload2.c
	$(COMPILE)

dswscope.o :        $(ACPICA_CORE)/dispatcher/dswscope.c
	$(COMPILE)

dswstate.o :        $(ACPICA_CORE)/dispatcher/dswstate.c
	$(COMPILE)

exconvrt.o :        $(ACPICA_CORE)/executer/exconvrt.c
	$(COMPILE)

excreate.o :        $(ACPICA_CORE)/executer/excreate.c
	$(COMPILE)

exdump.o :          $(ACPICA_CORE)/executer/exdump.c
	$(COMPILE)

exmisc.o :          $(ACPICA_CORE)/executer/exmisc.c
	$(COMPILE)

exmutex.o :         $(ACPICA_CORE)/executer/exmutex.c
	$(COMPILE)

exnames.o :         $(ACPICA_CORE)/executer/exnames.c
	$(COMPILE)

exoparg1.o :        $(ACPICA_CORE)/executer/exoparg1.c
	$(COMPILE)

exoparg2.o :        $(ACPICA_CORE)/executer/exoparg2.c
	$(COMPILE)

exoparg3.o :        $(ACPICA_CORE)/executer/exoparg3.c
	$(COMPILE)

exoparg6.o :        $(ACPICA_CORE)/executer/exoparg6.c
	$(COMPILE)

exprep.o :          $(ACPICA_CORE)/executer/exprep.c
	$(COMPILE)

exregion.o :        $(ACPICA_CORE)/executer/exregion.c
	$(COMPILE)

exresnte.o :        $(ACPICA_CORE)/executer/exresnte.c
	$(COMPILE)

exresolv.o :        $(ACPICA_CORE)/executer/exresolv.c
	$(COMPILE)

exresop.o :         $(ACPICA_CORE)/executer/exresop.c
	$(COMPILE)

exstore.o :         $(ACPICA_CORE)/executer/exstore.c
	$(COMPILE)

exstoren.o :        $(ACPICA_CORE)/executer/exstoren.c
	$(COMPILE)

exstorob.o :        $(ACPICA_CORE)/executer/exstorob.c
	$(COMPILE)

exsystem.o :        $(ACPICA_CORE)/executer/exsystem.c
	$(COMPILE)

exutils.o :         $(ACPICA_CORE)/executer/exutils.c
	$(COMPILE)

nsaccess.o :        $(ACPICA_CORE)/namespace/nsaccess.c
	$(COMPILE)

nsalloc.o :         $(ACPICA_CORE)/namespace/nsalloc.c
	$(COMPILE)

nsdump.o :          $(ACPICA_CORE)/namespace/nsdump.c
	$(COMPILE)

nsnames.o :         $(ACPICA_CORE)/namespace/nsnames.c
	$(COMPILE)

nsobject.o :        $(ACPICA_CORE)/namespace/nsobject.c
	$(COMPILE)

nsparse.o :         $(ACPICA_CORE)/namespace/nsparse.c
	$(COMPILE)

nssearch.o :        $(ACPICA_CORE)/namespace/nssearch.c
	$(COMPILE)

nsutils.o :         $(ACPICA_CORE)/namespace/nsutils.c
	$(COMPILE)

nswalk.o :          $(ACPICA_CORE)/namespace/nswalk.c
	$(COMPILE)

nsxfobj.o :         $(ACPICA_CORE)/namespace/nsxfobj.c
	$(COMPILE)

psargs.o :          $(ACPICA_CORE)/parser/psargs.c
	$(COMPILE)

psloop.o :          $(ACPICA_CORE)/parser/psloop.c
	$(COMPILE)

psopcode.o :        $(ACPICA_CORE)/parser/psopcode.c
	$(COMPILE)

psparse.o :         $(ACPICA_CORE)/parser/psparse.c
	$(COMPILE)

psscope.o :         $(ACPICA_CORE)/parser/psscope.c
	$(COMPILE)

pstree.o :          $(ACPICA_CORE)/parser/pstree.c
	$(COMPILE)

psutils.o :         $(ACPICA_CORE)/parser/psutils.c
	$(COMPILE)

pswalk.o :          $(ACPICA_CORE)/parser/pswalk.c
	$(COMPILE)

tbfadt.o :          $(ACPICA_CORE)/tables/tbfadt.c
	$(COMPILE)

tbinstal.o :        $(ACPICA_CORE)/tables/tbinstal.c
	$(COMPILE)

tbutils.o :         $(ACPICA_CORE)/tables/tbutils.c
	$(COMPILE)

tbxface.o :         $(ACPICA_CORE)/tables/tbxface.c
	$(COMPILE)

utalloc.o :         $(ACPICA_CORE)/utilities/utalloc.c
	$(COMPILE)

utcache.o :         $(ACPICA_CORE)/utilities/utcache.c
	$(COMPILE)

utcopy.o :          $(ACPICA_CORE)/utilities/utcopy.c
	$(COMPILE)

utdebug.o :         $(ACPICA_CORE)/utilities/utdebug.c
	$(COMPILE)

utdecode.o :        $(ACPICA_CORE)/utilities/utdecode.c
	$(COMPILE)

utdelete.o :        $(ACPICA_CORE)/utilities/utdelete.c
	$(COMPILE)

utglobal.o :        $(ACPICA_CORE)/utilities/utglobal.c
	$(COMPILE)

utinit.o :          $(ACPICA_CORE)/utilities/utinit.c
	$(COMPILE)

utlock.o :          $(ACPICA_CORE)/utilities/utlock.c
	$(COMPILE)

utmath.o :          $(ACPICA_CORE)/utilities/utmath.c
	$(COMPILE)

utmisc.o :          $(ACPICA_CORE)/utilities/utmisc.c
	$(COMPILE)

utmutex.o :         $(ACPICA_CORE)/utilities/utmutex.c
	$(COMPILE)

utobject.o :        $(ACPICA_CORE)/utilities/utobject.c
	$(COMPILE)

utresrc.o :         $(ACPICA_CORE)/utilities/utresrc.c
	$(COMPILE)

utstate.o :         $(ACPICA_CORE)/utilities/utstate.c
	$(COMPILE)

utxferror.o :       $(ACPICA_CORE)/utilities/utxferror.c
	$(COMPILE)

utxface.o :         $(ACPICA_CORE)/utilities/utxface.c
	$(COMPILE)


#
# Unix OS services layer (OSL)
#
osunixxf.o :        $(ACPICA_OSL)/osunixxf.c
	$(COMPILE)


clean :
	rm -f $(PROG) $(PROG).exe $(OBJS) $(INTERMEDIATES) $(MISC)

install :
	$(INSTALLPROG)

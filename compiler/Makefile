 

PROG=	iasl
SRCS=	aslcompilerparse.c aslcompilerlex.c aslanalyze.c aslcodegen.c \
	aslcompile.c aslerror.c aslfiles.c asllength.c \
	asllisting.c aslload.c asllookup.c aslmain.c aslmap.c aslopcodes.c \
	asloperands.c aslresource.c aslrestype1.c aslrestype2.c aslstartup.c \
	asltree.c aslutils.c asltransform.c aslfold.c aslstubs.c aslopt.c \
	../common/getopt.c \
	../utilities/utalloc.c \
	../utilities/utcache.c \
	../utilities/utcopy.c \
	../utilities/utdebug.c \
	../utilities/utdelete.c \
	../utilities/utglobal.c \
	../utilities/utinit.c \
	../utilities/utlock.c \
	../utilities/utobject.c \
	../utilities/utmisc.c \
	../utilities/utmath.c \
	../utilities/utmutex.c \
	../utilities/utresrc.c \
	../utilities/utstate.c \
	../utilities/utxface.c \
	../namespace/nsaccess.c \
	../namespace/nsalloc.c \
	../namespace/nsdump.c \
	../namespace/nsnames.c \
	../namespace/nsobject.c \
	../namespace/nsparse.c \
	../namespace/nssearch.c \
	../namespace/nsutils.c \
	../namespace/nswalk.c \
	../namespace/nsxfobj.c \
	../parser/psargs.c \
	../parser/psloop.c \
	../parser/psopcode.c \
	../parser/psparse.c \
	../parser/psscope.c \
	../parser/pstree.c \
	../parser/psutils.c \
	../parser/pswalk.c \
	../dispatcher/dswscope.c \
	../dispatcher/dswstate.c \
	../dispatcher/dsfield.c \
	../dispatcher/dsobject.c \
	../dispatcher/dsopcode.c \
	../dispatcher/dsutils.c \
	../dispatcher/dswexec.c \
	../dispatcher/dswload.c \
	../executer/exconvrt.c \
	../executer/excreate.c \
	../executer/exdump.c \
	../executer/exmisc.c \
	../executer/exmutex.c \
	../executer/exnames.c \
	../executer/exoparg1.c \
	../executer/exoparg2.c \
	../executer/exoparg3.c \
	../executer/exoparg6.c \
	../executer/exprep.c \
	../executer/exregion.c \
	../executer/exresnte.c \
	../executer/exresolv.c \
	../executer/exresop.c \
	../executer/exstore.c \
	../executer/exstoren.c \
	../executer/exstorob.c \
	../executer/exsystem.c \
	../executer/exutils.c \
	../common/adfile.c \
	../common/adisasm.c \
	../common/adwalk.c \
	../common/dmrestag.c \
	../common/dmtable.c \
	../common/dmtbinfo.c \
	../common/dmtbdump.c \
	../debugger/dbfileio.c \
	../disassembler/dmbuffer.c \
	../disassembler/dmnames.c \
	../disassembler/dmopcode.c \
	../disassembler/dmobject.c \
	../disassembler/dmresrc.c \
	../disassembler/dmresrcl.c \
	../disassembler/dmresrcs.c \
	../disassembler/dmutils.c \
	../disassembler/dmwalk.c \
	../tables/tbfadt.c \
	../tables/tbinstal.c \
	../tables/tbutils.c \
	../tables/tbxface.c \
	../osunixxf.c

NOMAN=	YES
CFLAGS+= -Wall -O2 -Wstrict-prototypes -D_LINUX -DACPI_ASL_COMPILER -I../include 

#YACC=	yacc
YACC=	bison
YFLAGS+= -v -d
LEX=	flex
LFLAGS+= -i

#.if $(YACC) == "bison"
YFLAGS+= -y -pAslCompiler
#.else
#CFLAGS+= -D_USE_BERKELEY_YACC
#.endif

LDLIBS = -lpthread -lrt

aslmain : $(patsubst %.c,%.o, $(SRCS))
	$(CC) $(LDFLAGS) $(patsubst %.c,%.o, $(SRCS)) \
		$(LOADLIBES) $(LDLIBS) -o iasl

CLEANFILES= y.output y.tab.c y.tab.h aslcompiler.y.h \
	aslcompilerparse.c aslcompilerlex.c iasl

aslcompilerparse.c: aslcompiler.y
	${YACC} ${YFLAGS} aslcompiler.y
	cp y.tab.c aslcompilerparse.c
	cp y.tab.h aslcompiler.y.h

aslcompilerlex.c: aslcompiler.l
	${LEX} ${LFLAGS} -PAslCompiler -oaslcompilerlex.c aslcompiler.l

clean : 
	rm -f $(CLEANFILES) $(patsubst %.c,%.o, $(SRCS))


#
# AcpiNames utility
#
PROG = acpinames

ACPICA_SRC =    ../..
ACPICA_COMMON = $(ACPICA_SRC)/common
ACPICA_CORE =   $(ACPICA_SRC)
ACPICA_TOOLS =  $(ACPICA_SRC)/tools
ACPICA_OSL =    $(ACPICA_SRC)/os_specific/service_layers

NOMAN=	YES
CFLAGS+= -Wall -g -D_LINUX -DNDEBUG -D_CONSOLE -DACPI_APPLICATION -DACPI_SINGLE_THREADED -DACPI_DEBUGGER -Wstrict-prototypes -I$(ACPICA_SRC)/include -I$(ACPICA_TOOLS)/acpinames

COMPILE = $(CC) -c $(CFLAGS) -o$@ $?
LDFLAGS += -lpthread -lrt

OBJS = \
	anmain.o \
	anstubs.o \
	antables.o \
	dbfileio.o \
	dsfield.o \
	dsmthdat.o \
	dsobject.o \
	dsutils.o \
	dswload.o \
	dswload2.o \
	dswscope.o \
	dswstate.o \
	excreate.o \
	exnames.o \
	exresnte.o \
	exresolv.o \
	exutils.o \
	getopt.o \
	nsaccess.o \
	nsalloc.o \
	nsdump.o \
	nsinit.o \
	nsload.o \
	nsnames.o \
	nsobject.o \
	nsparse.o \
	nssearch.o \
	nsutils.o \
	nswalk.o \
	nsxfeval.o \
	nsxfname.o \
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
	psxface.o \
	tbfadt.o \
	tbfind.o \
	tbinstal.o \
	tbutils.o \
	tbxface.o \
	tbxfroot.o \
	utalloc.o \
	utcache.o \
	utdebug.o \
	utdecode.o \
	utdelete.o \
	utglobal.o \
	utlock.o \
	utmath.o \
	utmisc.o \
	utmutex.o \
	utobject.o \
	utstate.o \
	utosi.o \
	utxferror.o \
	utxface.o


#
# Root rule
#
$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -lpthread -o $(PROG)
	$(COPYPROG)


#
# acpinames source
#
anmain.o :          $(ACPICA_TOOLS)/acpinames/anmain.c
	$(COMPILE)

anstubs.o :         $(ACPICA_TOOLS)/acpinames/anstubs.c
	$(COMPILE)

antables.o :        $(ACPICA_TOOLS)/acpinames/antables.c
	$(COMPILE)


#
# ACPICA core source - common
#
getopt.o :          $(ACPICA_COMMON)/getopt.c
	$(COMPILE)


#
# ACPICA core source
#
dbfileio.o :        $(ACPICA_CORE)/debugger/dbfileio.c
	$(COMPILE)

dsfield.o :         $(ACPICA_CORE)/dispatcher/dsfield.c
	$(COMPILE)

dsmthdat.o :        $(ACPICA_CORE)/dispatcher/dsmthdat.c
	$(COMPILE)

dsobject.o :        $(ACPICA_CORE)/dispatcher/dsobject.c
	$(COMPILE)

dsutils.o :         $(ACPICA_CORE)/dispatcher/dsutils.c
	$(COMPILE)

dswload.o :         $(ACPICA_CORE)/dispatcher/dswload.c
	$(COMPILE)

dswload2.o :        $(ACPICA_CORE)/dispatcher/dswload2.c
	$(COMPILE)

dswscope.o :        $(ACPICA_CORE)/dispatcher/dswscope.c
	$(COMPILE)

dswstate.o :        $(ACPICA_CORE)/dispatcher/dswstate.c
	$(COMPILE)

excreate.o :        $(ACPICA_CORE)/executer/excreate.c
	$(COMPILE)

exnames.o :         $(ACPICA_CORE)/executer/exnames.c
	$(COMPILE)

exresnte.o :        $(ACPICA_CORE)/executer/exresnte.c
	$(COMPILE)

exresolv.o :        $(ACPICA_CORE)/executer/exresolv.c
	$(COMPILE)

exutils.o :         $(ACPICA_CORE)/executer/exutils.c
	$(COMPILE)

nsaccess.o :        $(ACPICA_CORE)/namespace/nsaccess.c
	$(COMPILE)

nsalloc.o :         $(ACPICA_CORE)/namespace/nsalloc.c
	$(COMPILE)

nsdump.o :          $(ACPICA_CORE)/namespace/nsdump.c
	$(COMPILE)

nsinit.o :          $(ACPICA_CORE)/namespace/nsinit.c
	$(COMPILE)

nsload.o :          $(ACPICA_CORE)/namespace/nsload.c
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

nsxfeval.o :        $(ACPICA_CORE)/namespace/nsxfeval.c
	$(COMPILE)

nsxfname.o :        $(ACPICA_CORE)/namespace/nsxfname.c
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

psxface.o :         $(ACPICA_CORE)/parser/psxface.c
	$(COMPILE)

tbfadt.o :          $(ACPICA_CORE)/tables/tbfadt.c
	$(COMPILE)

tbfind.o :          $(ACPICA_CORE)/tables/tbfind.c
	$(COMPILE)

tbinstal.o :        $(ACPICA_CORE)/tables/tbinstal.c
	$(COMPILE)

tbutils.o :         $(ACPICA_CORE)/tables/tbutils.c
	$(COMPILE)

tbxface.o :         $(ACPICA_CORE)/tables/tbxface.c
	$(COMPILE)

tbxfroot.o :        $(ACPICA_CORE)/tables/tbxfroot.c
	$(COMPILE)

utalloc.o :         $(ACPICA_CORE)/utilities/utalloc.c
	$(COMPILE)

utcache.o :         $(ACPICA_CORE)/utilities/utcache.c
	$(COMPILE)

utdebug.o :         $(ACPICA_CORE)/utilities/utdebug.c
	$(COMPILE)

utdecode.o :        $(ACPICA_CORE)/utilities/utdecode.c
	$(COMPILE)

utdelete.o :        $(ACPICA_CORE)/utilities/utdelete.c
	$(COMPILE)

utglobal.o :        $(ACPICA_CORE)/utilities/utglobal.c
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

utstate.o :         $(ACPICA_CORE)/utilities/utstate.c
	$(COMPILE)

utosi.o :           $(ACPICA_CORE)/utilities/utosi.c
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
	rm -f $(PROG) $(PROG).exe $(OBJS)

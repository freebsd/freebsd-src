#
# acpinames - Load ACPI table and dump namespace. This is a subset
# of the AcpiExec functionality, it is intended to demonstrate
# the configurability of ACPICA.
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
PROG = acpinames

HOST =          _LINUX
NOMAN =         YES
COMPILE =       $(CC) -c $(CFLAGS) $(CWARNINGFLAGS) -o$@ $<

ACPICA_COMPONENTS =
ACPICA_SRC =            ../..
ACPICA_INCLUDE =        $(ACPICA_SRC)/include
ACPICA_COMMON =         $(ACPICA_SRC)/common
ACPICA_CORE =           $(ACPICA_SRC)$(ACPICA_COMPONENTS)
ACPICA_TOOLS =          $(ACPICA_SRC)/tools
ACPICA_OSL =            $(ACPICA_SRC)/os_specific/service_layers
ACPICA_DEBUGGER =       $(ACPICA_CORE)/debugger
ACPICA_DISPATCHER =     $(ACPICA_CORE)/dispatcher
ACPICA_EXECUTER =       $(ACPICA_CORE)/executer
ACPICA_NAMESPACE =      $(ACPICA_CORE)/namespace
ACPICA_PARSER =         $(ACPICA_CORE)/parser
ACPICA_TABLES =         $(ACPICA_CORE)/tables
ACPICA_UTILITIES =      $(ACPICA_CORE)/utilities
ACPINAMES =             $(ACPICA_TOOLS)/acpinames
INSTALLDIR =            /usr/bin
INSTALLPROG =           cp --remove-destination $(PROG) $(INSTALLDIR)

ACPICA_HEADERS = \
    $(wildcard $(ACPICA_INCLUDE)/*.h) \
    $(wildcard $(ACPICA_INCLUDE)/platform/*.h)

#
# Search paths for source files
#
vpath %.c \
    $(ACPINAMES) \
    $(ACPICA_DEBUGGER) \
    $(ACPICA_DISPATCHER) \
    $(ACPICA_EXECUTER) \
    $(ACPICA_NAMESPACE) \
    $(ACPICA_PARSER) \
    $(ACPICA_TABLES) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPINAMES)/*.h)

OBJECTS = \
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

CFLAGS+= \
    -D$(HOST) \
    -D_GNU_SOURCE \
    -DACPI_NAMES_APP \
    -I$(ACPICA_INCLUDE) \
    -I$(ACPINAMES)

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
# Rules
#
$(PROG) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROG)
	$(COPYPROG)

%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS)

install :
	$(INSTALLPROG)

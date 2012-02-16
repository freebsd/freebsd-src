#
# acpibin - Binary ACPI table utility
#
# NOTE: This makefile is intended to be used within the native
# ACPICA source tree.
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
PROG = acpibin

HOST =          _LINUX
NOMAN =         YES
COMPILE =       $(CC) -c $(CFLAGS) $(CWARNINGFLAGS) -o$@ $<

ACPICA_SRC =            ../../../source
ACPICA_COMMON =         $(ACPICA_SRC)/common
ACPICA_TOOLS =          $(ACPICA_SRC)/tools
ACPICA_OSL =            $(ACPICA_SRC)/os_specific/service_layers
ACPICA_CORE =           $(ACPICA_SRC)/components
ACPICA_INCLUDE =        $(ACPICA_SRC)/include
ACPICA_DEBUGGER =       $(ACPICA_CORE)/debugger
ACPICA_DISASSEMBLER =   $(ACPICA_CORE)/disassembler
ACPICA_DISPATCHER =     $(ACPICA_CORE)/dispatcher
ACPICA_EVENTS =         $(ACPICA_CORE)/events
ACPICA_EXECUTER =       $(ACPICA_CORE)/executer
ACPICA_HARDWARE =       $(ACPICA_CORE)/hardware
ACPICA_NAMESPACE =      $(ACPICA_CORE)/namespace
ACPICA_PARSER =         $(ACPICA_CORE)/parser
ACPICA_RESOURCES =      $(ACPICA_CORE)/resources
ACPICA_TABLES =         $(ACPICA_CORE)/tables
ACPICA_UTILITIES =      $(ACPICA_CORE)/utilities
ACPIBIN =               $(ACPICA_TOOLS)/acpibin
INSTALLDIR =            /usr/bin
INSTALLPROG =           cp --remove-destination $(PROG) $(INSTALLDIR)

ACPICA_HEADERS = \
    $(wildcard $(ACPICA_INCLUDE)/*.h) \
    $(wildcard $(ACPICA_INCLUDE)/platform/*.h)

#
# Search paths for source files
#
vpath %.c \
    $(ACPIBIN) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIBIN)/*.h)

OBJECTS = \
    abcompare.o \
    abmain.o \
	utalloc.o \
	utcache.o \
	utdebug.o \
	utdecode.o \
	utglobal.o \
	utlock.o \
	utmath.o \
	utmisc.o \
	utmutex.o \
	utstate.o \
	utxferror.o \
	osunixxf.o \
    getopt.o

CFLAGS+= \
    -D$(HOST) \
    -D_GNU_SOURCE \
    -DACPI_BIN_APP \
    -I$(ACPICA_INCLUDE)

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

%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS)

install :
	$(INSTALLPROG)

#
# acpibin - Binary ACPI table utility
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
PROG = acpibin

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

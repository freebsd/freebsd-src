##
## $Id: Makefile,v 1.31 1997/04/05 07:04:25 morgan Exp morgan $
## $FreeBSD$
##
## $Log: Makefile,v $
##
##

# major and minor numbers of this release
MAJOR_REL=0
MINOR_REL=65
DEBUG_REL=no
#DEBUG_REL=yes

# this should be the name of this directory
RELNAME = Linux-PAM-$(MAJOR_REL).$(MINOR_REL)

# this is the name of the archive file
DISTFILE = $(RELNAME).tar.gz

# define this to indicate to subdirectories that they are part of the 
# full source tree.
FULL_LINUX_PAM_SOURCE_TREE=yes
export FULL_LINUX_PAM_SOURCE_TREE

DYNLOAD="dl"
DYNTYPE="so"

# Comment out either line to disable that type of linking for *modules only*
# Both at once is a legal configuration!
DYNAMIC=-DPAM_DYNAMIC
#STATIC=-DPAM_STATIC

# Comment out these lines to disable building dynamic/static libpam.*
DYNAMIC_LIBPAM=yes
#STATIC_LIBPAM=yes

# All combinations of the above four variable definitions are legal,
# however, not defining either dynamic or static modules and yet
# creating a some flavor of LIBPAM will make an authentication library
# that always fails!

# Here we indicate which libraries are present on the local system
# they control the building of some modules in this distribution
# Note, these definitions are all "export"ed below...

HAVE_PWDBLIB=no
HAVE_CRACKLIB=no
HAVE_AFSLIBS=no
HAVE_KRBLIBS=no

# NB. The following is the generic defines for compilation.
# They can be overridden in the default.defs file below
#
WARNINGS = -ansi -D_POSIX_SOURCE -Wall -Wwrite-strings \
        -Wpointer-arith -Wcast-qual -Wcast-align \
        -Wtraditional -Wstrict-prototypes -Wmissing-prototypes \
        -Wnested-externs -Winline -Wshadow -pedantic
PIC=-fPIC

# Mode to install shared libraries with
SHLIBMODE=644

#
# Conditional defines..
#

ifdef DYNAMIC
# need the dynamic library functions
LIBDL=-l$(DYNLOAD)
ifdef STATIC_LIBPAM
# needed because pam_xxx() fn's are now in statically linked library
RDYNAMIC = -rdynamic
endif
endif

# Here we include the defines for the preferred operating system
# these include things like CC, CFLAGS and destination directories 
# etc.. By default, this is a symbolic link to one of the .defs files
# the .../defs/ directory. Please take a moment to check that you are
# using the correct one.

include default.defs

# to turn on the fprintf(stderr, ..) debugging lines throughout the
# distribution uncomment this line
#EXTRAS += -DDEBUG

# For serious memory allocation tracing uncomment the following
#MEMORY_DEBUG=-DMEMORY_DEBUG

#######################################################################
# The pam_unix module in this file will not work on NIS based systems.#
#######################################################################

# ////////////////////////////////////////////////////
# // You should not modify anything below this line //
# ////////////////////////////////////////////////////

# the sub-directories to make things in

DIRS = modules libpam conf libpam_misc examples

#
# basic defines
#

INCLUDEDIR=-I$(shell pwd)/include
PAMLIB=-L$(shell pwd)/libpam
PAMMISCLIB=-L$(shell pwd)/libpam_misc
ifeq ($(DEBUG_REL),yes)
 PAMLIB += -lpamd
 PAMMISCLIB += -lpamd_misc
else
 PAMLIB += -lpam
 PAMMISCLIB += -lpam_misc
endif


# This is Linux-PAM and not a version from Sun etc..
# [Note, this does not describe the operating system you are using
# only that you are compiling the "Linux" (read FREE) implementation
# of Pluggable Authentication Modules.
EXTRAS += -DLINUX_PAM

#
# build composite defines
#

LOADLIBES = $(PAMLIB) $(RDYNAMIC) $(PAMMISCLIB) $(LIBDL) $(ULIBS)

CFLAGS += $(EXTRAS) $(MEMORY_DEBUG) $(WARNINGS) $(INCLUDEDIR) $(PIC)
ifneq ($(strip $(OS)),)
CFLAGS += -D$(OS)
endif
ifneq ($(strip $(ARCH)),)
CFLAGS += -D$(ARCH)
endif

#
# export the libraries-available info; the modules should know how
# to deal with this...
#
export HAVE_PWDBLIB		
export HAVE_CRACKLIB
export HAVE_AFSLIBS
export HAVE_KRBLIBS

#
# generic exports
#
export MAJOR_REL                # the major release of this distribution
export MINOR_REL		# the minor release of this distribution
export DEBUG_REL		# for installing a debugging version of PAM
export OS			# operating system
export ARCH			# architecture
export CC			# the C compiler
export INSTALL			# to do instalations with
export MKDIR			# to ensure directories exist
export CFLAGS			# CC flags used to compile everything
export LD_D			# build a shared object file (module)
export LD_L			# build a shared library (e.g. libpam)
export USESONAME		# does shlib link command require soname option
export SOSWITCH			# shlib lib soname switch name
export NEEDSONAME		# does shared library link need versioned lib
export LD			# build a generic library
export LDCONFIG			# rebuild the shared libraries
export AR			# build a static library
export RANLIB			# reorder a static library
export LOADLIBES		# libraries needed for application linking
export PAMLIB			# where to find the local libpam.xx file
export DYNTYPE			# which suffix is used for libraries
export SHLIBMODE		# file mode for shared objects
#
# where to install things
#
export FAKEROOT			# for package maintainers
#
export PREFIX			# basic prefix for all other directories
export SUPLEMENTED		# where to store module helper binaries
export LIBDIR			# where libpam and libpam_misc go
export SECUREDIR		# where the modules will be placed
export INCLUDED			# where to store pam---.h files
export CONFIGED			# where pam.conf and pam.d/ go
export SCONFIGED		# where modules' config files go

#
# Conditional exporting ( ... these go on for a while... )
#
ifdef DYNAMIC 
export DYNAMIC
endif
ifdef STATIC
export STATIC
endif
ifdef DYNAMIC_LIBPAM
export DYNAMIC_LIBPAM
endif
ifdef STATIC_LIBPAM
export STATIC_LIBPAM
endif
ifdef MEMORY_DEBUG
export MEMORY_DEBUG
endif

##
## the rules
##

all: .freezemake
	@for i in $(DIRS) ; do \
		$(MAKE) -C $$i all ; \
		if [ $$? -ne 0 ]; then break ; fi ; \
	done

.freezemake:
# Do nothing

.old_freezemake: Makefile
	@touch .freezemake
	@echo "*WARNING*: If you are running a system that is dependent"
	@echo "  on PAM to work. DO NOT make sterile NOR make remove."
	@echo "  These options will delete the PAM files on your system"
	@echo "  and make it unusable!"
	@echo ""
	@echo "If you are in any doubt, just do 'make all' (or just"
	@echo "'make'). It is likely that this is the SAFEST thing to do...."
	@exit 1

install:
	@for i in $(DIRS) ; do \
		$(MAKE) -C $$i install ; \
		if [ $$? -ne 0 ]; then break ; fi ; \
	done
	install ./doc/man/*.3 $(PREFIX)/man/man3/
	install ./doc/man/*.8 $(PREFIX)/man/man8/

sterile: .freezemake
	@$(MAKE) remove
	@$(MAKE) extraclean

remove: .freezemake
	@for i in $(DIRS) ; do \
		$(MAKE) -C $$i remove ; \
	done

clean:
	@rm -f *~ core
	@for i in $(DIRS) ; do \
		$(MAKE) -C $$i clean ; \
	done

extraclean:
	@for i in $(DIRS) doc; do \
		$(MAKE) -C $$i extraclean ; \
	done

check:
	@$(MAKE) -C conf check

RCScheck:
	@$(MAKE) -C conf RCScheck

# this can be used to see what hasn't been check'd into RCS

open:
	@find . \( -type f -a -perm 644 \) -print

release:
	@egrep '^DEBUG\_REL\=yes' Makefile|grep -v grep > /dev/null ;\
		if [ $$? -eq 0 ]; then \
		echo "You should first set DEBUG_REL to no" ; exit 1 ; fi
	$(MAKE) extraclean
	rm -f .freezemake
	touch .filelist .RCSlist
	chmod 600 .filelist .RCSlist
	cd .. ; find $(RELNAME) \! -type d -print | fgrep -v RCS | fgrep -v 'conf/.md5sum' > $(RELNAME)/.filelist
	cd .. ; find $(RELNAME) -type f -print | fgrep RCS | fgrep -v 'conf/.RCSsum' > $(RELNAME)/.RCSlist
	chmod 400 .filelist .RCSlist
	$(MAKE) check
	$(MAKE) RCScheck
	(cat .filelist ; echo $(RELNAME)/conf/.md5sum) | (cd .. ; tar -cz -f$(DISTFILE) -T-)
	(cat .RCSlist ; echo $(RELNAME)/conf/.RCSsum) | (cd .. ; tar -cz -fRCS+$(DISTFILE) -T-)

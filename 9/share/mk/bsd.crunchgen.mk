#################################################################
#
# General notes:
#
# A number of Make variables are used to generate the crunchgen config file.
#
#  CRUNCH_SRCDIRS: lists directories to search for included programs
#  CRUNCH_PROGS:  lists programs to be included
#  CRUNCH_LIBS:  libraries to statically link with
#  CRUNCH_SHLIBS:  libraries to dynamically link with
#  CRUNCH_BUILDOPTS: generic build options to be added to every program
#  CRUNCH_BUILDTOOLS: lists programs that need build tools built in the
#       local architecture.
#
# Special options can be specified for individual programs
#  CRUNCH_SRCDIR_$(P): base source directory for program $(P)
#  CRUNCH_BUILDOPTS_$(P): additional build options for $(P)
#  CRUNCH_ALIAS_$(P): additional names to be used for $(P)
#
# By default, any name appearing in CRUNCH_PROGS or CRUNCH_ALIAS_${P}
# will be used to generate a hard link to the resulting binary.
# Specific links can be suppressed by setting
# CRUNCH_SUPPRESS_LINK_$(NAME) to 1.
#

# $FreeBSD$

##################################################################
#  The following is pretty nearly a generic crunchgen-handling makefile
#

CONF=	$(PROG).conf
OUTMK=	$(PROG).mk
OUTC=   $(PROG).c
OUTPUTS=$(OUTMK) $(OUTC) $(PROG).cache
CRUNCHOBJS= ${.OBJDIR}
.if defined(MAKEOBJDIRPREFIX)
CANONICALOBJDIR:= ${MAKEOBJDIRPREFIX}${.CURDIR}
.else
CANONICALOBJDIR:= /usr/obj${.CURDIR}
.endif

CLEANFILES+= $(CONF) *.o *.lo *.c *.mk *.cache *.a *.h

# Program names and their aliases contribute hardlinks to 'rescue' executable,
# except for those that get suppressed.
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
$(OUTPUTS): $(CRUNCH_SRCDIR_${P})/Makefile
.else
$(OUTPUTS): $(.CURDIR)/../../$(D)/$(P)/Makefile
.endif
.ifndef CRUNCH_SUPPRESS_LINK_${P}
LINKS+= $(BINDIR)/$(PROG) $(BINDIR)/$(P)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
.ifndef CRUNCH_SUPPRESS_LINK_${A}
LINKS+= $(BINDIR)/$(PROG) $(BINDIR)/$(A)
.endif
.endfor
.endfor
.endfor

all: $(PROG)
exe: $(PROG)

$(CONF): Makefile
	echo \# Auto-generated, do not edit >$(.TARGET)
.ifdef CRUNCH_BUILDOPTS
	echo buildopts $(CRUNCH_BUILDOPTS) >>$(.TARGET)
.endif
.ifdef CRUNCH_LIBS
	echo libs $(CRUNCH_LIBS) >>$(.TARGET)
.endif
.ifdef CRUNCH_SHLIBS
	echo libs_so $(CRUNCH_SHLIBS) >>$(.TARGET)
.endif
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
	echo progs $(P) >>$(.TARGET)
.ifdef CRUNCH_SRCDIR_${P}
	echo special $(P) srcdir $(CRUNCH_SRCDIR_${P}) >>$(.TARGET)
.else
	echo special $(P) srcdir $(.CURDIR)/../../$(D)/$(P) >>$(.TARGET)
.endif
.ifdef CRUNCH_BUILDOPTS_${P}
	echo special $(P) buildopts DIRPRFX=${DIRPRFX}${P}/ \
	    $(CRUNCH_BUILDOPTS_${P}) >>$(.TARGET)
.else
	echo special $(P) buildopts DIRPRFX=${DIRPRFX}${P}/ >>$(.TARGET)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
	echo ln $(P) $(A) >>$(.TARGET)
.endfor
.endfor
.endfor

# XXX Make sure we don't pass -P to crunchgen(1).
.MAKEFLAGS:= ${.MAKEFLAGS:N-P}
.ORDER: $(OUTPUTS) objs
$(OUTPUTS): $(CONF)
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} crunchgen -fq -m $(OUTMK) \
	    -c $(OUTC) $(CONF)

$(PROG): $(OUTPUTS) objs
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) exe

objs: $(OUTMK)
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) objs

# <sigh> Someone should replace the bin/csh and bin/sh build-tools with
# shell scripts so we can remove this nonsense.
build-tools:
.for _tool in $(CRUNCH_BUILDTOOLS)
	cd $(.CURDIR)/../../${_tool}; \
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} obj; \
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} build-tools
.endfor

# Use a separate build tree to hold files compiled for this crunchgen binary
# Yes, this does seem to partly duplicate bsd.subdir.mk, but I can't
# get that to cooperate with bsd.prog.mk.  Besides, many of the standard
# targets should NOT be propagated into the components.
cleandepend cleandir obj objlink:
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
	cd ${CRUNCH_SRCDIR_$(P)} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.else
	cd $(.CURDIR)/../../${D}/${P} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.endif
.endfor
.endfor

clean:
	rm -f ${CLEANFILES}
	if [ -e ${.OBJDIR}/$(OUTMK) ]; then				\
		MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) clean;	\
	fi
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
	cd ${CRUNCH_SRCDIR_$(P)} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.else
	cd $(.CURDIR)/../../${D}/${P} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.endif
.endfor
.endfor

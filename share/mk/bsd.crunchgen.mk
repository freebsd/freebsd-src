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
# If CRUNCH_GENERATE_LINKS is set to no, no links will be generated.
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
.elif defined(MAKEOBJDIR) && ${MAKEOBJDIR:M/*} != ""
CANONICALOBJDIR:=${MAKEOBJDIR}
.else
CANONICALOBJDIR:= /usr/obj${.CURDIR}
.endif
CRUNCH_GENERATE_LINKS?=	yes

CLEANFILES+= $(CONF) *.o *.lo *.c *.mk *.cache *.a *.h

# Don't try to extract debug info from ${PROG}.
MK_DEBUG_FILES=no

# Program names and their aliases contribute hardlinks to 'rescue' executable,
# except for those that get suppressed.
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
$(OUTPUTS): $(CRUNCH_SRCDIR_${P})/Makefile
.else
$(OUTPUTS): $(.CURDIR)/../../$(D)/$(P)/Makefile
.endif
.if ${CRUNCH_GENERATE_LINKS} == "yes"
.ifndef CRUNCH_SUPPRESS_LINK_${P}
LINKS+= $(BINDIR)/$(PROG) $(BINDIR)/$(P)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
.ifndef CRUNCH_SUPPRESS_LINK_${A}
LINKS+= $(BINDIR)/$(PROG) $(BINDIR)/$(A)
.endif
.endfor
.endif
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

CRUNCHGEN?= crunchgen
CRUNCHENV?= MK_TESTS=no
.ORDER: $(OUTPUTS) objs
$(OUTPUTS): $(CONF) .META
	MAKE=${MAKE} MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${CRUNCHGEN} -fq -m $(OUTMK) \
	    -c $(OUTC) $(CONF)

# These 2 targets cannot use .MAKE since they depend on the generated
# ${OUTMK} above.
$(PROG): $(OUTPUTS) objs
	${CRUNCHENV} MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) exe

objs: $(OUTMK)
	${CRUNCHENV} MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) objs

# <sigh> Someone should replace the bin/csh and bin/sh build-tools with
# shell scripts so we can remove this nonsense.
build-tools:
.for _tool in $(CRUNCH_BUILDTOOLS)
	${_+_}cd $(.CURDIR)/../../${_tool}; \
	${CRUNCHENV} MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} obj; \
	${CRUNCHENV} MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} build-tools
.endfor

# Use a separate build tree to hold files compiled for this crunchgen binary
# Yes, this does seem to partly duplicate bsd.subdir.mk, but I can't
# get that to cooperate with bsd.prog.mk.  Besides, many of the standard
# targets should NOT be propagated into the components.
cleandepend cleandir obj objlink:
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
	${_+_}cd ${CRUNCH_SRCDIR_$(P)} && \
	    ${CRUNCHENV} MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.else
	${_+_}cd $(.CURDIR)/../../${D}/${P} && \
	    ${CRUNCHENV} MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.endif
.endfor
.endfor

clean:
	rm -f ${CLEANFILES}
	${_+_}if [ -e ${.OBJDIR}/$(OUTMK) ]; then				\
		${CRUNCHENV} MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) clean;	\
	fi
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
	${_+_}cd ${CRUNCH_SRCDIR_$(P)} && \
	    ${CRUNCHENV} MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.else
	${_+_}cd $(.CURDIR)/../../${D}/${P} && \
	    ${CRUNCHENV} MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.endif
.endfor
.endfor

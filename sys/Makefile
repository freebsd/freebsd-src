#	From: /usr/src/sys/Makefile	V1.4			<stacey@guug.de>
#	$Id: Makefile,v 1.1 1994/01/22 07:34:02 rgrimes Exp $
#
#	Copyright Julian Stacey, Munich Oct. 1993, Free Software, No Liability
#	For Legalities see end of file.
#
# OVERVIEW:
#	Makefile & Makefile_sub.c are for the FreeBSD kernel usually
#	found in /usr/src/sys. They provide:
#	- Kernel configuration & make, defaulting to a kernel flavour
#	  automatically selected on a host name basis.
#	- Automatic config prior to make if configuration name changes.
#	- depend is done  automatically.
#	- Overide of default flavour by either "make CONFIG=whatever"
#	  or by editing default file.
#	- Selection precedence order & examples:
#		PRIORITY	METHOD
#		Highest:	make CONFIG=your_flavour
#	  else
#		Medium:		edit ${DEFAULT_FLAVOUR} contents to flavour
#	  else
#		Lowest:		Makefile internally does CONFIG=`hostname`
# TO SELECT YOUR FAVOURITE KERNEL FLAVOUR BY DEFAULT:
#	Move your favourite i386/conf configuration file to
#		 [/sys/]i386/conf/`hostname`
#	Examples of how to force compilation of non hostname configurations:
#		a)	make CONFIG=LINT
#		b)	echo "GENERICAH" > .config.default ; make
# EXTERNAL ENVIRONMENT VARIABLES:
#	MACHINE		Mandatory.	Must be preset before make is invoked,
#					example: i386.
#	DESTDIR		Optional.	If preset in the environment, 
#					it is recognised.
#	BOOT_SYS	Optional.	Set to enable makes in the i386/boot
#					(mdec) sub directory,
#					(may later be un-necessary,
#					if we use SUBDIR=${MACHINE}/boot).
#	BOOT_FLAG	Optional.	Set to null (with "BOOT_FLAG=") to
#					allow makes in i386/boot, 
#					(overrides the default make -n).
#	CONFIG		Optional	Usage as shown above.
#	DEBUG_SYS	Optional	Preset to debug makefile.
# REQUIREMENTS: bsd-make, a cpp.
# PENDING:
#	obj: label needs to be filled out:
#		Till then binaries get compiled in source tree,
#		which isnt nice for following reasons:
#		- If source is on a cd-rom,
#		- If source is on a networked source tree serving
#		  several simultaneous compiles
#		Local variants of Makefile_sub .config.now, .config.last
#		.config.makefile will collide).
#		Work around problem for now by ensuring each compile is
#		done in its own seperate writable source tree, with symbolic
#		links pointing to each source file (including such as 
#		Makefile & Makefile_sub.c)
#	Review whether I still need .config.import in
#	Makefile & Makefile_sub.c
# BUGS:	If old config that does not recognise config "386bsd" at 0x...
#	complains of syntax error, no error code is received
#	the make then later fails.
# -----------------------------------------------------------------------------
# MAINTENANCE NOTES
# - If changing names ensure consistency in Makefile & Makefile_sub.c.
# - I do not use a method whereby I would assume a local hostname in
#   DEFAULT_FLAVOUR, instead I take `hostname` if it is empty
#	(as distributed) this avoids problems of:
#	a) source tree copied unclean to other systems
#	b) source tree shared by several nfs mounted hosts,
# - Cpp is used to generate the $(MAKEFILE_KERNEL), to get dependency labels
#   (to the left of the colon) such as CONFIG_TREE_TEST to be recognised
#   by freebsd make  (I couldnt in a simple makefile use labels like:
#   compile/${THIS_FLAVOUR}/386bsd: )
# - The driving source is simply called Makefile to allow default
#   make rules to work when recursing down tree from /usr/src/Makefile.
# - make builds a dependency list at startup,
#   timestamps are also checked at startup, not on the fly.
#   so we have to run a second make to notice if ${FORCE_MAKEFILE} has been
#   touched by commands within label `config_makefile`.
# - constructs such as:
#	foo:	`bar`
#   won't work.
# - Cannot have $(MAKEFILE_KERNEL):	${MACHINE}/conf/`cat ${THIS_FLAVOUR}`
#   as `cat ${THIS_FLAVOUR}` would not evaluate.
# ----------------------------------------------------------------------------
# INTERNAL ENVIRONMENT VARIABLES, & FILE NAMES:
#	Changeable by user (but if changed, do a make cleandir first, and
#	leave no gap between assigned variable string and # comment delimiter !
KERNEL_NAME=	386bsd#		Kernel Name
DEFAULT_FLAVOUR= .config.default# Contains name of what to make by default
THIS_FLAVOUR=	.config.now#	Contains what to make now, (a temp variable).
LAST_FLAVOUR=	.config.last#	What MAKEFILE_KERNEL was last produced for.
FORCE_MAKEFILE=	.config.makefile# If new MAKEFILE_KERNEL to be forcibly built.
BOOT_FLAG= -n	# Prevents real make in i386/boot/, Can be overridden by bsd_make BOOT_FLAG=
# ----------------------------------------------------------------------------
# INTERNAL ENVIRONMENT VARIABLES:
#	Do NOT change these without reading Makefile & Makefile_sub.c.
#	Do not add gaps between assigned variable string and # comment delimiter !
MAKEFILE_KERNEL=Makefile_sub#	also used to refer to Makefile_sub.c
TREE_TEST=	Makefile#	used as:	compile/CONFIG_NAME/$TREE_TEST,
#				( typically compile/GENERICAH/Makefile )
#				compared with config file date.
#				swap386bsd.c is newer, but not sure if
#				swap${MACHINE}.c would be an architecture
#				independent portable name.
.PATH=		# No directories searched for files not found in current dir.
CONFIG:=	`hostname`#	Can be overridden by bsd_make CONFIG=whatever
CWD!=		pwd
INFORM=		echo
.if defined(DEBUG_SYS)
CALL_BEFORE=	" /sys/makefile label "
CALL_AFTER=	" has been invoked."
.else
# Suppress chat about what config being made etc. 
DIVERT=		> /dev/null
.endif
BOOT_START="Start considering (with make ${BOOT_FLAG}) what could be done in ${MACHINE}/boot:"
BOOT_END="End of considering ${MACHINE}/boot."
TRANSIENT=	${LAST_FLAVOUR} ${THIS_FLAVOUR} \
		${FORCE_MAKEFILE} ${MAKEFILE_KERNEL} .config.import
#		Transient temporary files, created by config_makefile.

# =============================================================================

# EXTERNALLY CALLABLE LABELS

#	This label is callable from /usr/src/Makefile.
all:	all_start config_tree compile_tree boot_all all_end

#	This label is callable from /usr/src/Makefile,
#	Clean current-flavour compile tree, but not other flavours.
clean:	clean_start lifebelt config_makefile boot_clean
	@${INFORM} "Cleaning compile tree for configuration name `cat $(THIS_FLAVOUR)`". ${DIVERT}
	@echo "==={> compile/`cat $(CWD)/$(THIS_FLAVOUR)`"
	-(cd ./compile; cd ./`cat $(CWD)/$(THIS_FLAVOUR)`; make clean)
	@echo "<}=== compile/`cat $(CWD)/$(THIS_FLAVOUR)`"
#	# cd is done in 2 stages in case directory named in $(THIS_FLAVOUR)
#	# is not there, else make would recurse endlessly.
#	# A more drastic alternative to the above make clean could be:
#	#	-rm -r -f ./compile/`cat ./$(THIS_FLAVOUR)`/*
	@${INFORM} Removing makefile temporary files. ${DIVERT}
	-rm -f ${TRANSIENT}
#	# Dont move "rm TRANSIENT" before "rm compile/`cat ${THIS_FLAVOUR}`"
	@${INFORM} To do more extensive cleaning, use make cleandir ${DIVERT}

#	This label is callable from /usr/src/Makefile,
cleandir:	cleandir_start boot_cleandir
	@${INFORM} Removing all kernel compile trees. ${DIVERT}
	-rm -f -r compile/*
	-rm -f ${TRANSIENT}
	@echo "Warning: Removing $(DEFAULT_FLAVOUR)."
	@echo Future makes will default to generate a flavour for `hostname`.
	-rm -f $(DEFAULT_FLAVOUR)
	@${INFORM} To also clean kernel in destination tree, use make clean_dest ${DIVERT}

clean_dest:
	@${INFORM} Cleaning destination tree kernel. ${DIVERT}
	-rm -f $(DESTDIR)/${KERNEL_NAME}.untested.`cat ${THIS_FLAVOUR}`
	-rm -f $(DESTDIR)/config.untested.`cat ${THIS_FLAVOUR}`
#	# Suprisingly the - before the rm above is needed for
#	# $(DESTDIR)/${KERNEL_NAME}.untested.${CONFIG_NAME} else make clean
#	# fails, yet rm exit value bug supposed to have been fixed way back.

#	This label is callable from /usr/src/Makefile,
depend: boot_depend
	@${INFORM} "${CALL_BEFORE}'all'${CALL_AFTER}" ${DIVERT}
	@echo This a dummy depend label in sys/Makefile to satisfy possible callers.
#	# Currently the config depend gets done from `all', later
#	# when I've thought more about it, maybe I'll add `depend:'
#	# to be dependent on `config_tree'.
#	# As dependencies get evaluated anyway, & as /sys/make call 
#	# make depend

#	This label is callable from /usr/src/Makefile,
obj:	obj_start boot_obj
	@echo "NOT YET DEVELOPED AN obj ENTRY FOR /sys/Makefile."
	@echo "So compiles are in source directory."
	@echo Do not attempt to share a common source directory with others,
	@echo "as transient files will conflict (eg ${TRANSIENT})"

#	This label is callable from /usr/src/Makefile,
install:	install_start all boot_install
	install -c -m 644 compile/`cat $(THIS_FLAVOUR)`/${KERNEL_NAME} \
		$(DESTDIR)/${KERNEL_NAME}.untested.`cat $(THIS_FLAVOUR)`
#	# Beware: If you remove the word untested, & then `cat $(THIS_FLAVOUR)`
#	# does not evaluate properly, you will overwrite your current
#	# working kernel.
	install -c -m 644 ${MACHINE}/conf/`cat $(THIS_FLAVOUR)` \
		$(DESTDIR)/config.untested.`cat $(THIS_FLAVOUR)`
	sync	# ensure complete kernel in root even if update dead
	@${INFORM} "Looking at kernel sizes (> 593920 may overlap floppy buffers):" ${DIVERT}
	-find $(DESTDIR)/$(KERNEL_NAME)* \! -name \*.gz -print | xargs size
	@${INFORM} -n "For local notes on kernel sizes etc:" ${DIVERT}
	@${INFORM} -n "	cat ${MACHINE}/doc/`cat $(THIS_FLAVOUR)`" ${DIVERT}
	@${INFORM} -n To test: fasthalt, wait, select	$(DESTDIR)/${KERNEL_NAME}.untested. ${DIVERT}
	@${INFORM} "`cat ${THIS_FLAVOUR}`" ${DIVERT}

# =============================================================================
# LABELS BELOW HERE ARE INTERNAL, (not to be called from outside this Makefile)
# =============================================================================
lifebelt:
	@${INFORM} "make clean decides what flavour to clean in a rather complex" ${DIVERT}
	@${INFORM} "way using tools such as cpp. If make clean breaks" ${DIVERT}
	@${INFORM} "call the more simplistic make cleandir." ${DIVERT}

clean_start cleandir_start all_start install_start obj_start all_end:
	@${INFORM} "${CALL_BEFORE}$@${CALL_AFTER}" ${DIVERT}

# -----------------------------------------------------------------------------
# STUFF TO SUPERCEDE ../Makefile mdec:
# (maybe to be in turn superceeded by SUBDIR=${MACHINE}/boot

boot_all:
	@${INFORM} ${BOOT_START} ${DIVERT}
.if defined (BOOT_SYS)
.if ${MACHINE} == "i386"
#	# mdec work is called from /usr/src/Makefile, this was done before
#	# this /usr/src/sys/Makefile was written, boot_all work could/should
#	# now be moved down to here
#	# (but not until people have had time to object)
	@${INFORM} /usr/src/Makefile currently deals with mdec work. ${DIVERT}
	@echo "==={> ${MACHINE}/boot"
	cd ./${MACHINE}/boot; make ${BOOT_FLAG} depend all install ${CLEANDIR}
	@echo "<}=== ${MACHINE}/boot"
.if defined (DESTDIR)
	@${INFORM} -n "NOT DOING:	" ${DIVERT}
	@${INFORM} "cd /usr/mdec; find . | cpio -pdamuv ${DESTDIR}/usr/mdec" ${DIVERT}
#	# The above is what /usr/src/Makefile does,
#	# however it should be improved,
#	# because boot blocks going to DESTDIR should come from the obj tree,
#	# not from out current live /usr/mdec.
#	# cpio flags:
#		-a:	Reset the access times of files after reading them,
#			so that it does not look like they have just been
#			read.
#		-d:	Create leading directories where needed.
#		-m:	Retain previous file modification times when creat-
#			ing files.
#		-p:	Run in copy-pass mode.
#		-u:	Replace all files, without asking whether to
#			replace existing newer files with older files.
#		-v:	List the files processed, or with -t, give an `ls
#			-l' style table of contents listing. In a verbose
#			table of contents of a ustar archive, user and
#			group names in the archive that do not exist on the
#			local system are replaced by the names that corre-
#			spond locally to the numeric UID and GID stored in
#			the archive.
.endif
.endif
.endif
	@${INFORM} ${BOOT_END} ${DIVERT}

boot_install: boot_start
.if defined (BOOT_SYS)
	@echo "==={> ${MACHINE}/boot"
	cd ./${MACHINE}/boot; make ${BOOT_FLAG} install
	@echo "<}=== ${MACHINE}/boot"
.endif
	@${INFORM} ${BOOT_END} ${DIVERT}

boot_clean: boot_start
.if defined (BOOT_SYS)
	@echo "==={> ${MACHINE}/boot"
	cd ./${MACHINE}/boot; make ${BOOT_FLAG} clean
	@echo "<}=== ${MACHINE}/boot"
.endif
	@${INFORM} ${BOOT_END} ${DIVERT}

boot_cleandir: boot_start
.if defined (BOOT_SYS)
	@echo "==={> ${MACHINE}/boot"
	cd ./${MACHINE}/boot; make ${BOOT_FLAG} cleandir
	@echo "<}=== ${MACHINE}/boot"
.endif
	@${INFORM} ${BOOT_END} ${DIVERT}

boot_depend: boot_start
.if defined (BOOT_SYS)
	@echo "==={> ${MACHINE}/boot"
	cd ./${MACHINE}/boot; make ${BOOT_FLAG} depend
	@echo "<}=== ${MACHINE}/boot"
.endif
	@${INFORM} ${BOOT_END} ${DIVERT}

boot_obj: boot_start
.if defined (BOOT_SYS)
	@echo "==={> ${MACHINE}/boot"
	cd ./${MACHINE}/boot; make ${BOOT_FLAG} obj
	@echo "<}=== ${MACHINE}/boot"
.endif
	@${INFORM} ${BOOT_END} ${DIVERT}

boot_start:
	@${INFORM} ${BOOT_START} ${DIVERT}

# -----------------------------------------------------------------------------
# Stuff to determine which flavour kernel to compile.

config_which:
	@${INFORM} Determining which kernel flavour to make. ${DIVERT}
	@${INFORM} CONFIG variable is $(CONFIG). ${DIVERT}
.if exists(${DEFAULT_FLAVOUR})
	@${INFORM} "${DEFAULT_FLAVOUR} exists" ${DIVERT}
	@${INFORM} Seeing if ${DEFAULT_FLAVOUR} has contents. ${DIVERT}
	if ( XX==X`cat ${DEFAULT_FLAVOUR}`X ); \
	then \
		${INFORM} "${DEFAULT_FLAVOUR} is empty" ${DIVERT} ; \
		echo "$(CONFIG)" > ${THIS_FLAVOUR}; \
	else \
		${INFORM} "${DEFAULT_FLAVOUR} is not empty" ${DIVERT} ; \
		echo "`cat ${DEFAULT_FLAVOUR}`" > ${THIS_FLAVOUR}; \
	fi
.else
	@${INFORM} "${DEFAULT_FLAVOUR} not extant." ${DIVERT}
.if defined(CONFIG)
		@${INFORM} "CONFIG Makefile variable in ${THIS_FLAVOUR} set to ${CONFIG}." ${DIVERT}
		@echo ${CONFIG} > ${THIS_FLAVOUR}
.else
		@${INFORM} "CONFIG Makefile variable unset" ${DIVERT}
		@${INFORM} "Assuming existence of $(MACHINE)/conf/`hostname`" ${DIVERT}
		hostname > ${THIS_FLAVOUR}
.endif
.endif
	@${INFORM} "Flavour to be worked on is `cat ${THIS_FLAVOUR}`." ${DIVERT}
.if !exists(${LAST_FLAVOUR})
	@${INFORM} "Creating a dummy ${LAST_FLAVOUR} as no previous make" ${DIVERT}
	cp /dev/null ${LAST_FLAVOUR}
#	# ${LAST_FLAVOUR} is used to sense change of configuration to be made.
.endif
# -----------------------------------------------------------------------------
# Decide if Makefile_sub should be remade.
config_makefile:	config_which $(MAKEFILE_KERNEL)
	@${INFORM} Determining if $(MAKEFILE_KERNEL) needs to be remade. ${DIVERT}
.if !exists(${FORCE_MAKEFILE})
	touch ${FORCE_MAKEFILE} ; 
#	# This touch only needs to be in the "then", not the "else",
#	# however getting it there causes problems with backslash delimeters.
.endif
	if ( XX==X`cmp ${LAST_FLAVOUR} ${THIS_FLAVOUR}`X ); \
	then \
		${INFORM} -n "Same configuration name as last time," ${DIVERT} ; \
		${INFORM} " no need to remake $(MAKEFILE_KERNEL)." ${DIVERT} ; \
	else \
		${INFORM} -n "Configuration changed from " ${DIVERT} ; \
		${INFORM} "`cat ${LAST_FLAVOUR}` to `cat ${THIS_FLAVOUR}`." ${DIVERT} ; \
		${INFORM} "Forcing a new $(MAKEFILE_KERNEL)" ${DIVERT} ; \
		touch ${FORCE_MAKEFILE}; \
		echo "==={> $(MAKEFILE_KERNEL)" ; \
		make $(MAKEFILE_KERNEL); \
		echo "<}=== $(MAKEFILE_KERNEL)" ; \
	fi
# -----------------------------------------------------------------------------
# Make a compile tree (of sources).
.if defined(LOAD_ADDRESS)
T_LD_ADR=LOAD_ADDRESS=${LOAD_ADDRESS}
.else
T_LD_ADR=
.endif
.if defined(DEBUG_SYS)
T_DBG=DEBUG_SYS=YES
.else
T_DBG=
.endif
config_tree: config_makefile
	@${INFORM} Determining if compile tree needs to be remade. ${DIVERT}
	@echo "==={> $(MAKEFILE_KERNEL)"
	make -f $(MAKEFILE_KERNEL) ${T_DBG} ${T_LD_ADR} \
		compile/`cat ${THIS_FLAVOUR}`/${TREE_TEST}
	@echo "<}=== $(MAKEFILE_KERNEL)"
# -----------------------------------------------------------------------------
# Create a file who's sole purpose is to be a timestamp,
# if this file is newer than $(MAKEFILE_KERNEL), then $(MAKEFILE_KERNEL)
# needs to be remade.
${FORCE_MAKEFILE}:
	touch $@
# -----------------------------------------------------------------------------
# Make Makefile_sub.
$(MAKEFILE_KERNEL): Makefile ${MAKEFILE_KERNEL}.c ${FORCE_MAKEFILE}
	@${INFORM} "${CALL_BEFORE}$@${CALL_AFTER}" ${DIVERT}
	cpp -P -DCONFIG_NAME=`cat ${THIS_FLAVOUR}` -Ui386 \
		-DCONFIG_TREE_TEST=compile/`cat ${THIS_FLAVOUR}`/${TREE_TEST} \
		-DMAKEFILE_KERNEL=${MAKEFILE_KERNEL} \
		${MAKEFILE_KERNEL}.c > $(MAKEFILE_KERNEL) \
#	# -Ui386 avoids damage to names like .../i386/conf/...
	cp ${THIS_FLAVOUR} ${LAST_FLAVOUR}
# -----------------------------------------------------------------------------
# Compile a tree.
compile_tree:
	@${INFORM} $(MAKEFILE) label $@ ${DIVERT}
	@${INFORM} "Making kernel compilation tree for `cat ${THIS_FLAVOUR}`." ${DIVERT}
	@echo "==={> compile/`cat ${THIS_FLAVOUR}`"
	cd ./compile/`cat ${THIS_FLAVOUR}`; make
	@echo "<}=== compile/`cat ${THIS_FLAVOUR}`"
# -----------------------------------------------------------------------------
.include <bsd.subdir.mk>
# Maybe a subsidiary make in compile/something or i386/boot will want bsd.subdir.mk,
# but Makefile_sub.c does not need it, (neither does it cause harm).
# -----------------------------------------------------------------------------
# LEGALITIES	(as refered to from top of file).
#
# Copyright (c) 1993 Julian Stacey, Munich, Germany. 
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# This software is provided by Julian Stacey (and subsequent 
# contributors) ``as is'' and
# any express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular purpose
# are disclaimed.  In no event shall Julian Stacey (and subsequent
# contributors) be liable
# for any direct, indirect, incidental, special, exemplary, or consequential
# damages (including, but not limited to, procurement of substitute goods
# or services; loss of use, data, or profits; or business interruption)
# however caused and on any theory of liability, whether in contract, strict
# liability, or tort (including negligence or otherwise) arising in any way
# out of the use of this software, even if advised of the possibility of
# such damage.

# End of Makefile.

#	End of Makefile

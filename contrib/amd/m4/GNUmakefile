#
# THIS MAKEFILE IS FOR am-utils maintainers only!
#
# GNUmakefile for remaking configuration files.
#
# Erez Zadok <ezk@cs.columbia.edu>
#

AUTOCONF=autoconf --localdir=./m4 ./m4/configure.in
AUTOHEADER=autoheader --localdir=./m4 ./m4/configure.in
#AUTOMAKE=./m4/automake --include-deps --amdir=./m4/amdir
#AUTOMAKE=automake --include-deps --no-intermediates --localdir=./m4
#AUTOMAKE=automake --include-deps --localdir=./m4
AUTOMAKE=automake --include-deps --altdir=./m4
ACLOCAL=aclocal --altdir=./m4
MK_ACLOCAL=./mk-aclocal

CUTWARNMSG="warning: AC_TRY_RUN called without default to allow cross compilin"
CUTWARNMSG2="AC_OUTPUT_COMMANDS|autoupdate"
AMFILES=../Makefile.am $(wildcard ../*/Makefile.am)
INAMFILES=$(AMFILES:.am=.in)
ACFILES=$(wildcard macros/*.m4 macros/HEADER macros/TRAILER m4/aclocal.m4)

LOG=/tmp/amu-${USER}.log

TOPDIR=..
VPATH=

all: testdir ${TOPDIR}/configure config.h.in ${TOPDIR}/Makefile.in \
	GNUmakefile stamp-h.in

config: all $(TOPDIR)/buildall aux_conf.h.in
	(cd ${TOPDIR} && ./buildall -c)

dconfig: all $(TOPDIR)/buildall aux_conf.h.in
	(cd ${TOPDIR} && ./buildall -C)

qconfig: all $(TOPDIR)/buildall aux_conf.h.in
	(cd ${TOPDIR} && ./buildall -q)

build: all $(TOPDIR)/buildall aux_conf.h.in
	(cd ${TOPDIR} && ./buildall -b)

world: all $(TOPDIR)/buildall aux_conf.h.in
	(cd ${TOPDIR} && ./buildall)

testdir: ${TOPDIR}/amd/amd.c

${TOPDIR}/configure: configure.in aclocal.m4
	-(cd ${TOPDIR} && ${AUTOCONF} > configure.new 2> ${LOG})
	-@egrep -v ${CUTWARNMSG} ${LOG} > ${LOG}.new ; mv ${LOG}.new ${LOG}
	@echo '############################################################'
	@echo 'Checking for "AC_OUTPUT_COMMANDS" warnings:'
	@echo ' (If you see any output, fix this when autoconf/automake are'
	@echo '  back in sync in their CVS trees. -ezk, 2/10/00)'
	-@egrep ${CUTWARNMSG2} ${LOG}
	-@egrep -v ${CUTWARNMSG2} ${LOG} > ${LOG}.new ; mv ${LOG}.new ${LOG}
	@echo '############################################################'
	@if test -s ${LOG}; then cat ${LOG}; exit 2; fi
	-mv ../configure ../configure.old
	mv ../configure.new ../configure
	rm -f ../configure.old
	chmod a+rx $@

config.h.in: configure.in acconfig.h
	-(cd ${TOPDIR} && ${AUTOHEADER} > ./m4/config.h.in 2> ${LOG})
	-@egrep -v ${CUTWARNMSG} ${LOG} > ${LOG}.new ; mv ${LOG}.new ${LOG}
	@echo '############################################################'
	@echo 'Checking for "AC_OUTPUT_COMMANDS" warnings:'
	@echo ' (If you see any output, fix this when autoconf/automake are'
	@echo '  back in sync in their CVS trees. -ezk, 2/10/00)'
	-@egrep ${CUTWARNMSG2} ${LOG}
	-@egrep -v ${CUTWARNMSG2} ${LOG} > ${LOG}.new ; mv ${LOG}.new ${LOG}
	@echo '############################################################'
	@if test -s ${LOG}; then cat ${LOG}; exit 2; fi

${TOPDIR}/Makefile.in: ${AMFILES} configure.in aclocal.m4
	(cd ${TOPDIR} && ${AUTOMAKE})
	@rm -f ${LOG}

aclocal.m4: ${ACFILES} configure.in
	rm -f $@
	${MK_ACLOCAL} > acinclude.m4
	(cd ${TOPDIR} && ${ACLOCAL})

stamp-h.in: ${AMFILES} config.h.in aclocal.m4 aux_conf.h.in
	echo timestamp > $@

clean:
	rm -f ${TOPDIR}/configure config.h.in $(INAMFILES) aclocal.m4 stamp.h.in

##############################################################################
# maintainer rules to update autoconf/automake/libtool files distributed with
# am-utils. -Erez.

P1s=/usr/local/gnu/share/automake
P2s=/usr/local/gnu/share/libtool
P1=/usr/local/gnu/lib/automake
P2=/usr/local/gnu/lib/libtool
P3=/misc/mirror/gnu/prep/config
P4=/misc/mirror/gnu/prep
ICMD=./m4/copy-if-newbig
CPCMD=cp -p

update:		\
	config.guess	\
	config.sub	\
	doc/texinfo.tex
# 	depcomp		\
# 	install-sh	\
# 	ltconfig	\
# 	ltmain.sh	\
# 	mdate-sh	\
# 	missing		\
# 	mkinstalldirs	\

config.guess:: $(P1s)/config.guess
	$(ICMD) $? $@
config.guess:: $(P2s)/config.guess
	$(ICMD) $? $@
config.guess:: $(P3)/config.guess
	$(ICMD) $? $@

config.sub:: $(P1s)/config.sub
	$(ICMD) $? $@
config.sub:: $(P2s)/config.sub
	$(ICMD) $? $@
config.sub:: $(P3)/config.sub
	$(ICMD) $? $@

depcomp:: $(P1)/depcomp
	$(CPCMD) $? $@

install-sh: $(P1)/install-sh
	$(CPCMD) $? $@

ltconfig: $(P2)/ltconfig
	$(CPCMD) $? $@

ltmain.sh: $(P2)/ltmain.sh
	$(CPCMD) $? $@

mdate-sh: $(P1)/mdate-sh
	$(CPCMD) $? $@

missing: $(P1)/missing
	$(CPCMD) $? $@

mkinstalldirs: $(P1)/mkinstalldirs
	$(CPCMD) $? $@

doc/texinfo.tex: $(P4)/texinfo.tex
	$(CPCMD) $? $@

##############################################################################

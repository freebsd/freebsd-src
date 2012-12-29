divert(-1)
#
# Copyright (c) 1999-2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: manpage.m4,v 8.16 2012/01/21 00:07:08 ashish Exp $
#
divert(0)dnl

define(`bldGET_MAN_SOURCE_NUM',
`substr($1, eval(len($1) - 1))'dnl
)dnl
define(`bldGET_MAN_BASE_NAME',
`substr($1, 0, eval(len($1) - 2))'dnl
)dnl
ifdef(`confNO_MAN_BUILD',, `
bldPUSH_TARGET(`${MANPAGES}')
bldPUSH_INSTALL_TARGET(`install-docs')')
bldLIST_PUSH_ITEM(`bldMAN_PAGES', `bldSOURCES')dnl

MANOWN=	ifdef(`confMANOWN', `confMANOWN', `bin')
MANGRP=	ifdef(`confMANGRP', `confMANGRP', `bin')
MANMODE=ifdef(`confMANMODE', `confMANMODE', `444')
MANROOT=ifdef(`confMANROOT', `confMANROOT', `/usr/share/man/cat')
MANROOTMAN=ifdef(`confMANROOTMAN', `confMANROOTMAN', `/usr/share/man/man')
MAN1=	${MANROOT}ifdef(`confMAN1', `confMAN1', `1')
MAN1MAN=${MANROOTMAN}ifdef(`confMAN1', `confMAN1', `1')
MAN1EXT=ifdef(`confMAN1EXT', `confMAN1EXT', `1')
MAN1SRC=ifdef(`confMAN1SRC', `confMAN1SRC', `0')
MAN3=	${MANROOT}ifdef(`confMAN3', `confMAN3', `3')
MAN3MAN=${MANROOTMAN}ifdef(`confMAN3', `confMAN3', `3')
MAN3EXT=ifdef(`confMAN3EXT', `confMAN3EXT', `3')
MAN3SRC=ifdef(`confMAN3SRC', `confMAN3SRC', `0')
MAN4=	${MANROOT}ifdef(`confMAN4', `confMAN4', `4')
MAN4MAN=${MANROOTMAN}ifdef(`confMAN4', `confMAN4', `4')
MAN4EXT=ifdef(`confMAN4EXT', `confMAN4EXT', `4')
MAN4SRC=ifdef(`confMAN4SRC', `confMAN4SRC', `0')
MAN5=	${MANROOT}ifdef(`confMAN5', `confMAN5', `5')
MAN5MAN=${MANROOTMAN}ifdef(`confMAN5', `confMAN5', `5')
MAN5EXT=ifdef(`confMAN5EXT', `confMAN5EXT', `5')
MAN5SRC=ifdef(`confMAN5SRC', `confMAN5SRC', `0')
MAN8=	${MANROOT}ifdef(`confMAN8', `confMAN8', `8')
MAN8MAN=${MANROOTMAN}ifdef(`confMAN8', `confMAN8', `8')
MAN8EXT=ifdef(`confMAN8EXT', `confMAN8EXT', `8')
MAN8SRC=ifdef(`confMAN8SRC', `confMAN8SRC', `0')

define(`bldMAN_TARGET_NAME',
`bldGET_MAN_BASE_NAME($1).${MAN`'bldGET_MAN_SOURCE_NUM($1)`SRC}' 'dnl
)dnl
MANPAGES= bldFOREACH(`bldMAN_TARGET_NAME(', `bldMAN_PAGES')

divert(bldTARGETS_SECTION)
define(`bldMAN_BUILD_CMD',
`bldGET_MAN_BASE_NAME($1).${MAN`'bldGET_MAN_SOURCE_NUM($1)`SRC}': bldGET_MAN_BASE_NAME($1).bldGET_MAN_SOURCE_NUM($1)
	${NROFF} ${MANDOC} bldGET_MAN_BASE_NAME($1).bldGET_MAN_SOURCE_NUM($1) > bldGET_MAN_BASE_NAME($1)`.${MAN'bldGET_MAN_SOURCE_NUM($1)`SRC}' || ${CP} bldGET_MAN_BASE_NAME($1)`.${MAN'bldGET_MAN_SOURCE_NUM($1)`SRC}'.dist bldGET_MAN_BASE_NAME($1)`.${MAN'bldGET_MAN_SOURCE_NUM($1)`SRC}''

)dnl
bldFOREACH(`bldMAN_BUILD_CMD(', `bldMAN_PAGES')

install-docs: ${MANPAGES}
ifdef(`confNO_MAN_INSTALL', `divert(-1)', `dnl')
define(`bldMAN_INSTALL_CMD',
`ifdef(`confDONT_INSTALL_CATMAN', `dnl',
`	ifdef(`confMKDIR', `if [ ! -d ${DESTDIR}${MAN'bldGET_MAN_SOURCE_NUM($1)`SRC} ]; then confMKDIR -p ${DESTDIR}${MAN'bldGET_MAN_SOURCE_NUM($1)`SRC}; else :; fi ')
	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} bldGET_MAN_BASE_NAME($1).`${MAN'bldGET_MAN_SOURCE_NUM($1)`SRC}' `${DESTDIR}${MAN'bldGET_MAN_SOURCE_NUM($1)}/bldGET_MAN_BASE_NAME($1)`.${MAN'bldGET_MAN_SOURCE_NUM($1)`EXT}'')
ifdef(`confINSTALL_RAWMAN',
`	ifdef(`confMKDIR', `if [ ! -d ${DESTDIR}${MAN'bldGET_MAN_SOURCE_NUM($1)`MAN} ]; then confMKDIR -p ${DESTDIR}${MAN'bldGET_MAN_SOURCE_NUM($1)`MAN}; else :; fi ')
	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} bldGET_MAN_BASE_NAME($1).bldGET_MAN_SOURCE_NUM($1) `${DESTDIR}${MAN'bldGET_MAN_SOURCE_NUM($1)`MAN}'/bldGET_MAN_BASE_NAME($1)`.${MAN'bldGET_MAN_SOURCE_NUM($1)`EXT}'', `dnl')'
)dnl
bldFOREACH(`bldMAN_INSTALL_CMD(', `bldMAN_PAGES')
ifdef(`confNO_MAN_INSTALL', `divert(0)', `dnl')
divert(0)

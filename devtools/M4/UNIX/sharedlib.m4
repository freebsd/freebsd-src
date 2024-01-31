divert(-1)
#
# Copyright (c) 2000-2001, 2006, 2008 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: sharedlib.m4,v 8.19 2013-11-22 20:51:23 ca Exp $
#
divert(0)dnl

define(`confLIBEXT', `a')dnl

SHAREDLIB_SUFFIX= ifdef(`confSHAREDLIB_SUFFIX', `confSHAREDLIB_SUFFIX', `')
SHAREDLIB_EXT= ifdef(`confSHAREDLIB_EXT', `confSHAREDLIB_EXT', `.so')
SHAREDLIB= bldCURRENT_PRODUCT${SHAREDLIB_EXT}${SHAREDLIB_SUFFIX}
SHAREDLIB_LINK= bldCURRENT_PRODUCT${SHAREDLIB_EXT}
SHAREDLIBDIR= ifdef(`confSHAREDLIBDIR',`confSHAREDLIBDIR',`/usr/lib/')
DEPLIBS= ifdef(`confDEPLIBS', `confDEPLIBS', `') ${bldCURRENT_PRODUCT`SMDEPLIBS'}

CONFIG_SONAME= ifdef(`confSONAME', `confSONAME ${SHAREDLIB}', `')

include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/links.m4')dnl
bldLIST_PUSH_ITEM(`bldC_PRODUCTS', bldCURRENT_PRODUCT)dnl
bldPUSH_TARGET(${SHAREDLIB})dnl
bldPUSH_TARGET(bldCURRENT_PRODUCT`.a')dnl
bldPUSH_INSTALL_TARGET(`install-'bldCURRENT_PRODUCT)dnl
bldPUSH_CLEAN_TARGET(bldCURRENT_PRODUCT`-clean')dnl

ifdef(`bld_ALREADY_SO',,
	`ifdef(`confCCOPTS_SO',
		`PREPENDDEF(`confCCOPTS', `defn(`confCCOPTS_SO')')')')
define(`bld_ALREADY_SO')
include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/defines.m4')
divert(bldTARGETS_SECTION)

${SHAREDLIB}: ${BEFORE} ${bldCURRENT_PRODUCT`OBJS'} ifelse(bldOS, `AIX', `bldCURRENT_PRODUCT.a')
	${LD} ${LDOPTS_SO} ${CONFIG_SONAME} -o ${SHAREDLIB} ${bldCURRENT_PRODUCT`OBJS'} ${LIBDIRS} ${DEPLIBS}
	ifelse(bldOS, `AIX', `${CP} ${SHAREDLIB} shr.o
	${AR} ${AROPTS} bldCURRENT_PRODUCT.a shr.o
	${CP} bldCURRENT_PRODUCT.a ${SHAREDLIB}',`rm -f bldCURRENT_PRODUCT${SHAREDLIB_EXT}
	${LN} ${SHAREDLIB} bldCURRENT_PRODUCT${SHAREDLIB_EXT}')

bldCURRENT_PRODUCT.a: ${BEFORE} ${bldCURRENT_PRODUCT`OBJS'}
	${AR} ${AROPTS} bldCURRENT_PRODUCT.a ${bldCURRENT_PRODUCT`OBJS'}
	${RANLIB} ${RANLIBOPTS} bldCURRENT_PRODUCT.a

ifdef(`bldLINK_SOURCES', `bldMAKE_SOURCE_LINKS(bldLINK_SOURCES)')

install-`'bldCURRENT_PRODUCT: ${SHAREDLIB}
	ifdef(`confMKDIR', `if [ ! -d ${DESTDIR}${SHAREDLIBDIR} ]; then confMKDIR -p ${DESTDIR}${SHAREDLIBDIR}; else :; fi ')
	${INSTALL} -c -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${SHAREDLIB} ${DESTDIR}${SHAREDLIBDIR}
	ifelse(bldOS, `AIX', `${AR} ${AROPTS} ${DESTDIR}${SHAREDLIBDIR}bldCURRENT_PRODUCT.a ${SHAREDLIB}', `rm -f ${DESTDIR}${SHAREDLIBDIR}${SHAREDLIB_LINK}
	${LN} ${LNOPTS} ${DESTDIR}${SHAREDLIBDIR}${SHAREDLIB} ${DESTDIR}${SHAREDLIBDIR}${SHAREDLIB_LINK}')

bldCURRENT_PRODUCT-clean:
	rm -f ${OBJS} ${SHAREDLIB} bldCURRENT_PRODUCT.a ${MANPAGES} ifelse(bldOS, `AIX', `shr.o', `bldCURRENT_PRODUCT${SHAREDLIB_EXT}')

divert(0)

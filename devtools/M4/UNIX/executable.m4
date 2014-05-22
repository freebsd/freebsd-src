divert(-1)
#
# Copyright (c) 1999, 2001, 2006 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: executable.m4,v 8.25 2013-11-22 20:51:22 ca Exp $
#
divert(0)dnl
include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/links.m4')dnl
bldLIST_PUSH_ITEM(`bldC_PRODUCTS', bldCURRENT_PRODUCT)dnl
bldPUSH_TARGET(bldCURRENT_PRODUCT)dnl
bldPUSH_INSTALL_TARGET(`install-'bldCURRENT_PRODUCT)dnl
bldPUSH_CLEAN_TARGET(bldCURRENT_PRODUCT`-clean')dnl
bldPUSH_ALL_SRCS(bldCURRENT_PRODUCT`SRCS')dnl
bldPUSH_STRIP_TARGET(`strip-'bldCURRENT_PRODUCT)dnl

include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/defines.m4')
divert(bldTARGETS_SECTION)
bldCURRENT_PRODUCT: ${bldCURRENT_PRODUCT`OBJS'} ${bldCURRENT_PRODUCT`SMDEPLIBS'}
	${CCLINK} -o bldCURRENT_PRODUCT ${LDOPTS} ${LIBDIRS} ${bldCURRENT_PRODUCT`OBJS'} ${LIBS}

ifdef(`bldLINK_SOURCES', `bldMAKE_SOURCE_LINKS(bldLINK_SOURCES)')

ifdef(`bldNO_INSTALL', ,
`install-`'bldCURRENT_PRODUCT: bldCURRENT_PRODUCT ifdef(`bldTARGET_INST_DEP', `bldTARGET_INST_DEP')
	ifdef(`confMKDIR', `if [ ! -d ${DESTDIR}${bldINSTALL_DIR`'BINDIR} ]; then confMKDIR -p ${DESTDIR}${bldINSTALL_DIR`'BINDIR}; else :; fi ')
	${INSTALL} -c -o ${bldBIN_TYPE`'BINOWN} -g ${bldBIN_TYPE`'BINGRP} -m ${bldBIN_TYPE`'BINMODE} bldCURRENT_PRODUCT ${DESTDIR}${bldINSTALL_DIR`'BINDIR}
ifdef(`bldTARGET_LINKS', `bldMAKE_TARGET_LINKS(${bldINSTALL_DIR`'BINDIR}/bldCURRENT_PRODUCT, ${bldCURRENT_PRODUCT`'TARGET_LINKS})')')

strip-`'bldCURRENT_PRODUCT: bldCURRENT_PRODUCT
	${STRIP} ${STRIPOPTS} ${DESTDIR}${bldINSTALL_DIR`'BINDIR}`'/bldCURRENT_PRODUCT

bldCURRENT_PRODUCT-clean:
	rm -f ${OBJS} bldCURRENT_PRODUCT ${MANPAGES}
divert(0)

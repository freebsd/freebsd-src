divert(-1)
#
# Copyright (c) 1999-2001, 2006 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: library.m4,v 8.12 2013-11-22 20:51:23 ca Exp $
#
divert(0)dnl
include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/links.m4')dnl
bldLIST_PUSH_ITEM(`bldC_PRODUCTS', bldCURRENT_PRODUCT)dnl
bldPUSH_TARGET(bldCURRENT_PRODUCT`.a')dnl
bldPUSH_INSTALL_TARGET(`install-'bldCURRENT_PRODUCT)dnl
bldPUSH_CLEAN_TARGET(bldCURRENT_PRODUCT`-clean')dnl

include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/defines.m4')
divert(bldTARGETS_SECTION)
bldCURRENT_PRODUCT.a: ${BEFORE} ${bldCURRENT_PRODUCT`OBJS'}
	${AR} ${AROPTS} bldCURRENT_PRODUCT.a ${bldCURRENT_PRODUCT`OBJS'}
	${RANLIB} ${RANLIBOPTS} bldCURRENT_PRODUCT.a
ifdef(`bldLINK_SOURCES', `bldMAKE_SOURCE_LINKS(bldLINK_SOURCES)')

install-`'bldCURRENT_PRODUCT: bldCURRENT_PRODUCT.a
ifdef(`bldINSTALLABLE', `	ifdef(`confMKDIR', `if [ ! -d ${DESTDIR}${bldINSTALL_DIR`'LIBDIR} ]; then confMKDIR -p ${DESTDIR}${bldINSTALL_DIR`'LIBDIR}; else :; fi ')
	${INSTALL} -c -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} bldCURRENT_PRODUCT.a ${DESTDIR}${LIBDIR}')

bldCURRENT_PRODUCT-clean:
	rm -f ${OBJS} bldCURRENT_PRODUCT.a ${MANPAGES}

divert(0)

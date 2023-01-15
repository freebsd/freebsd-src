divert(-1)
#
# Copyright (c) 2006 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#	Compile/run a test program.
#
#	$Id: check.m4,v 8.6 2013-11-22 20:51:22 ca Exp $
#
divert(0)dnl
divert(-1)
define(`smcheck', `dnl
ifelse(X`'$2, `X', `', `ifelse(index($2, `run'), `-1', `', `dnl
bldLIST_PUSH_ITEM(`bldCHECK_TARGETS', $1)dnl
')')
ifelse(X`'$2, `X', `', `ifelse(index($2, `compile'), `-1', `', `dnl
bldLIST_PUSH_ITEM(`bldC_CHECKS', $1)dnl
bldLIST_PUSH_ITEM(`bldCHECK_PROGRAMS', $1)dnl
bldPUSH_CLEAN_TARGET($1`-clean')dnl
divert(bldTARGETS_SECTION)
$1`'SRCS=$1.c
$1: ${BEFORE} $1.o ifdef(`confCHECK_LIBS', `confCHECK_LIBS')
	${CC} -o $1 ${LDOPTS} ${LIBDIRS} $1.o ifdef(`confCHECK_LIBS', `confCHECK_LIBS') ${LIBS}
$1-clean:
	rm -f $1 $1.o')')
divert(0)')

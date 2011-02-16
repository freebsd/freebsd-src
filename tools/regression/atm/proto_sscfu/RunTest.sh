#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscfu/RunTest.sh,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscfu

$LOCALBASE/bin/ats_sscfu $options $DATA/Funcs $DATA/EST* $DATA/REL* \
$DATA/REC* $DATA/RES* $DATA/DATA* $DATA/UDATA*

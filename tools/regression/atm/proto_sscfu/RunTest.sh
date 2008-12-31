#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscfu/RunTest.sh,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscfu

$LOCALBASE/bin/ats_sscfu $options $DATA/Funcs $DATA/EST* $DATA/REL* \
$DATA/REC* $DATA/RES* $DATA/DATA* $DATA/UDATA*

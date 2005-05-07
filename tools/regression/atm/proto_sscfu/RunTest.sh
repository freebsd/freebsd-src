#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscfu/RunTest.sh,v 1.1 2004/01/29 16:01:57 harti Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscfu

$LOCALBASE/bin/ats_sscfu $options $DATA/Funcs $DATA/EST* $DATA/REL* \
$DATA/REC* $DATA/RES* $DATA/DATA* $DATA/UDATA*
